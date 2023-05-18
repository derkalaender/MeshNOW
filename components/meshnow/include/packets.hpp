#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "constants.hpp"

// Forward declaration because of circular dependency
namespace meshnow {
class Networking;
struct ReceiveMeta;
}  // namespace meshnow

namespace meshnow::packets {

enum class Type : uint8_t {
    // HEALTH
    STILL_ALIVE,  ///< Periodically sent by all nodes to detect dead nodes

    // HANDSHAKE
    ANYONE_THERE,  ///< Sent by a node trying to connect to the mesh
    I_AM_HERE,     ///< Sent by nodes already in the mesh in reply to AnyoneThere
    PLS_CONNECT,   ///< Sent by a node to request a connection to another specific node
    VERDICT,       ///< Sent by a node to accept/decline a connection request

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

    MAX,  ///< Number of packet types. Meta, should not actually be used
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

    /**
     * Calls the corresponding handler function for this payload.
     */
    virtual void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const = 0;
};

/**
 * A MeshNOW packet.
 * Consists of a header (magic bytes and payload type) and a payload.
 */
struct Packet {
    // Constant magic bytes to identify meshnow packets
    constexpr static std::array<uint8_t, 3> MAGIC{0x55, 0x77, 0x55};

    explicit Packet(const BasePayload& payload) : payload_{payload} {}

    /**
     * Serializes the packet into a byte buffer.
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserializes the packet payload from a byte buffer.
     */
    static std::unique_ptr<BasePayload> deserialize(const std::vector<uint8_t>& buffer);

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

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;
};

struct AnyoneTherePayload : DumbPayload {
    AnyoneTherePayload() : DumbPayload(Type::ANYONE_THERE) {}

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;
};

struct IAmHerePayload : DumbPayload {
    IAmHerePayload() : DumbPayload(Type::I_AM_HERE) {}

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;
};

struct PlsConnectPayload : DumbPayload {
    PlsConnectPayload() : DumbPayload(Type::PLS_CONNECT) {}

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;
};

struct VerdictPayload : BasePayload {
    explicit VerdictPayload(bool accept_connection, MAC_ADDR root_mac)
        : accept_connection_{accept_connection}, root_mac_{root_mac} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::VERDICT; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const bool accept_connection_;

    const MAC_ADDR root_mac_;
};

struct NodeConnectedPayload : BasePayload {
    explicit NodeConnectedPayload(MAC_ADDR connected_to) : connected_to_{connected_to} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::NODE_CONNECTED; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const MAC_ADDR connected_to_;
};

struct NodeDisconnectedPayload : BasePayload {
    explicit NodeDisconnectedPayload(MAC_ADDR disconnected_from) : disconnected_from_{disconnected_from} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::NODE_DISCONNECTED; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const MAC_ADDR disconnected_from_;
};

struct MeshUnreachablePayload : DumbPayload {
    MeshUnreachablePayload() : DumbPayload(Type::MESH_UNREACHABLE) {}

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;
};

struct MeshReachablePayload : DumbPayload {
    MeshReachablePayload() : DumbPayload(Type::MESH_REACHABLE) {}

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;
};

struct DataAckPayload : BasePayload {
    explicit DataAckPayload(const MAC_ADDR target, uint16_t seq_num) : target_{target}, seq_num_{seq_num} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::DATA_ACK; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const MAC_ADDR target_;
    const uint16_t seq_num_;
};

struct DataNackPayload : BasePayload {
    explicit DataNackPayload(const MAC_ADDR target, uint16_t seq_num) : target_{target}, seq_num_{seq_num} {}

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return Type::DATA_NACK; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const MAC_ADDR target_;
    const uint16_t seq_num_;
};

struct DataFirstPayload : BasePayload {
    explicit DataFirstPayload(const MAC_ADDR target, uint16_t seq_num, uint16_t len, const bool custom,
                              const std::vector<uint8_t> data)
        : target_{target}, seq_num_{seq_num}, len_{len}, custom_{custom}, data_{data} {
        assert(len >= 1 && len <= MAX_DATA_TOTAL_SIZE);
        assert(!data.empty());
        assert(data.size() <= MAX_DATA_FIRST_SIZE);
    }

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return custom_ ? Type::DATA_CUSTOM_FIRST : Type::DATA_LWIP_FIRST; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const MAC_ADDR target_;
    const uint16_t seq_num_;
    const uint16_t len_;
    const bool custom_;
    const std::vector<uint8_t> data_;
};

struct DataNextPayload : BasePayload {
    explicit DataNextPayload(const MAC_ADDR target, uint16_t seq_num, uint8_t frag_num, const bool custom,
                             const std::vector<uint8_t> data)
        : target_{target}, seq_num_{seq_num}, frag_num_{frag_num}, custom_{custom}, data_{data} {
        assert(frag_num >= 1 && frag_num < MAX_FRAG_NUM);
        assert(!data.empty());
        assert(data.size() <= MAX_DATA_NEXT_SIZE);
    }

    void serialize(std::vector<uint8_t>& buffer) const override;

    size_t serializedSize() const override;

    Type type() const override { return custom_ ? Type::DATA_CUSTOM_NEXT : Type::DATA_LWIP_NEXT; }

    void handle(meshnow::Networking& networking, const ReceiveMeta& meta) const override;

    const MAC_ADDR target_;
    const uint16_t seq_num_;
    const uint8_t frag_num_;
    const bool custom_;
    const std::vector<uint8_t> data_;
};

}  // namespace meshnow::packets
