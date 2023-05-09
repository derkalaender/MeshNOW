#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "constants.hpp"
#include "esp_log.h"
#include "queue.hpp"

namespace meshnow {

/**
 * Handles networking.
 * <br>
 * Terminology:<br>
 * - Root: The root node is the only one potentially connected to a router and to that extend the internet. Coordinates
 * the whole network. Should be connected to a power source.<br>
 * - Node: If not otherwise specified, a "node" refers to any node that is not the root. Can be a leaf node or a branch
 * node.<br>
 * - Parent: Each node (except for the root) has exactly one parent node.<br>
 * - Child: Each node can have multiple children.<br>
 */
class Networking {
   public:
    Networking() { receive_thread = std::thread(&Networking::ReceiveWorker, this); }

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    /**
     * Send callback for ESP-NOW.
     */
    void on_send(const uint8_t* mac_addr, esp_now_send_status_t status);

    /**
     * Receive callback for ESP-NOW.
     */
    void on_receive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len);

    // TODO private
   public:
    struct RecvData {
        MAC_ADDR src_addr;
        MAC_ADDR dest_addr;
        uint8_t rssi;
        std::vector<uint8_t> data;
    };

    /**
     * Broadcasts a raw payload to all nearby devices, no matter if connected/part of the mesh or not.
     * @param payload data to send
     *
     * @note Payloads larger than MAX_RAW_PACKET_SIZE will throw an exception.
     */
    static void raw_broadcast(const std::vector<uint8_t>& payload);

    /**
     * Sends a raw payload to a specific device (ESP-NOW wrapper).
     * @param mac_addr the MAC address of the device to send to
     * @param payload data to send
     *
     * @note Payloads larger than MAX_RAW_PACKET_SIZE will throw an exception.
     */
    static void raw_send(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& payload);

    /**
     * Handles the receive receive_queue.
     */
    [[noreturn]] void ReceiveWorker();

    std::thread receive_thread;

    util::Queue<RecvData> receive_queue{RECEIVE_QUEUE_SIZE};
};

namespace packet {

enum class Type : uint8_t {
    // HEALTH
    STILL_ALIVE,  ///< Periodically sent by all nodes to detect dead nodes

    // HANDSHAKE
    ANYONE_THERE,  ///< Sent by a node trying to connect to the mesh
    I_AM_HERE,     ///< Sent by nodes already in the mesh in reply to AnyoneThere
    PLS_CONNECT,   ///< Sent by a node to request a connection to another specific node
    WELCOME,       ///< Sent by a node to accept a connection request

    // EVENT
    NODE_CONNECTED,     ///< Sent by a parent when a new child connects, bubbles up
    NODE_DISCONNECTED,  ///< Sent by a parent when a child disconnects, bubbles up
    MESH_UNREACHABLE,   ///< Sent by a node to its children when it loses connection to its parent, propagates down
    MESH_REACHABLE,     ///< Sent by a node to its children when it regains connection to its parent, propagates down

    // DATA
    DATA_ACK,   ///< Sent by the target node to acknowledge a complete datagram
    DATA_NACK,  ///< Sent by any immediate parent of the current hop to indicate the inability to accept/forward the
                ///< datagram (e.g. out of memory)

    DATA_LWIP_FIRST,  ///< TCP/IP data
    DATA_LWIP_NEXT,

    DATA_CUSTOM_FIRST,  ///< Custom data
    DATA_CUSTOM_NEXT,
};

/**
 * Actual payload of a packet.
 * Polymorphism is sick.
 */
struct BasePayload {
    virtual ~BasePayload() = default;

    /**
     * Serializes the payload into a byte array.
     */
    virtual void serialize(std::vector<uint8_t>& buffer) const = 0;

    /**
     * Size of the payload in bytes.
     */
    virtual size_t serializedSize() const = 0;

    /**
     * Type of the payload.
     */
    virtual Type type() const = 0;
};

/**
 * A MeshNOW packet.
 * Consists of a header (magic bytes and payload type) and a payload.
 */
struct Packet {
    // Constant magic bytes to identify meshnow packets
    constexpr static std::array<uint8_t, 3> MAGIC{0x55, 0x77, 0x55};

    explicit Packet(BasePayload& payload) : payload_{payload} {}

    /**
     * Serializes the packet into a byte buffer.
     */
    std::vector<uint8_t> serialize() const;

   private:
    // Payload data
    const BasePayload& payload_;
};

/**
 * Payloads that don't contain any data.
 */
struct DumbPayload : BasePayload {
    explicit DumbPayload(Type type) : type_{type} {}

    void serialize(std::vector<uint8_t>& buffer) const override {}

    size_t serializedSize() const override { return 0; }

    Type type() const override { return type_; }

    const Type type_;
};

struct StillAlivePayload : DumbPayload {
    StillAlivePayload() : DumbPayload(Type::STILL_ALIVE) {}
};

struct AnyoneTherePayload : DumbPayload {
    AnyoneTherePayload() : DumbPayload(Type::ANYONE_THERE) {}
};

struct IAmHerePayload : DumbPayload {
    IAmHerePayload() : DumbPayload(Type::I_AM_HERE) {}
};

struct PlsConnectPayload : DumbPayload {
    PlsConnectPayload() : DumbPayload(Type::PLS_CONNECT) {}
};

struct WelcomePayload : DumbPayload {
    WelcomePayload() : DumbPayload(Type::WELCOME) {}
};

struct NodeConnectedPayload : BasePayload {
    explicit NodeConnectedPayload(const MAC_ADDR& connected_to) : connected_to_{connected_to} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::NODE_CONNECTED; }

    const MAC_ADDR& connected_to_;
};

struct NodeDisconnectedPayload : BasePayload {
    explicit NodeDisconnectedPayload(const MAC_ADDR& disconnected_from) : disconnected_from_{disconnected_from} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::NODE_DISCONNECTED; }

    const MAC_ADDR& disconnected_from_;
};

struct MeshUnreachablePayload : DumbPayload {
    MeshUnreachablePayload() : DumbPayload(Type::MESH_UNREACHABLE) {}
};

struct MeshReachablePayload : DumbPayload {
    MeshReachablePayload() : DumbPayload(Type::MESH_REACHABLE) {}
};

struct DataAckPayload : BasePayload {
    explicit DataAckPayload(const MAC_ADDR& target, uint16_t seq_num) : target_{target}, seq_num_{seq_num} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::DATA_ACK; }

    const MAC_ADDR& target_;
    const uint16_t seq_num_;
};

struct DataNackPayload : BasePayload {
    explicit DataNackPayload(const MAC_ADDR& target, uint16_t seq_num) : target_{target}, seq_num_{seq_num} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::DATA_NACK; }

    const MAC_ADDR& target_;
    const uint16_t seq_num_;
};

struct DataFirstPayload : BasePayload {
    explicit DataFirstPayload(const MAC_ADDR& target, uint16_t seq_num, uint16_t len, const bool custom,
                              const std::vector<uint8_t>& data)
        : target_{target}, seq_num_{seq_num}, len_{len}, custom_{custom}, data_{data} {
        assert(len >= 1 && len <= MAX_DATA_TOTAL_SIZE);
        assert(!data.empty());
        assert(data.size() <= MAX_DATA_FIRST_SIZE);
    }

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return custom_ ? Type::DATA_CUSTOM_FIRST : Type::DATA_LWIP_FIRST; }

    const MAC_ADDR& target_;
    const uint16_t seq_num_;
    const uint16_t len_;
    const bool custom_;
    const std::vector<uint8_t>& data_;
};

struct DataNextPayload : BasePayload {
    explicit DataNextPayload(const MAC_ADDR& target, uint16_t seq_num, uint8_t frag_num, const bool custom,
                             const std::vector<uint8_t>& data)
        : target_{target}, seq_num_{seq_num}, frag_num_{frag_num}, custom_{custom}, data_{data} {
        assert(frag_num >= 1 && frag_num <= MAX_FRAG_NUM);
        assert(!data.empty());
        assert(data.size() <= MAX_DATA_NEXT_SIZE);
    }

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return custom_ ? Type::DATA_CUSTOM_NEXT : Type::DATA_LWIP_NEXT; }

    const MAC_ADDR& target_;
    const uint16_t seq_num_;
    const uint8_t frag_num_;
    const bool custom_;
    const std::vector<uint8_t>& data_;
};

}  // namespace packet
}  // namespace meshnow
