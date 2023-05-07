#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "constants.hpp"
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
        std::vector<uint8_t>* data;
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
    DATA_ACK,     ///< Sent by the target node to acknowledge a complete datagram
    DATA_NACK,    ///< Sent by any immediate parent of the current hop to indicate the inability to
                  ///< accept/forward the datagram (e.g. out of memory)
    DATA_LWIP,    ///< TCP/IP data
    DATA_CUSTOM,  ///< Custom data
};

class Common {
   public:
    // Constant magic bytes to identify meshnow packets
    constexpr static std::array<uint8_t, 3> MAGIC{0x55, 0x77, 0x55};

    /**
     * Serializes the packet into a byte array.
     * @return
     */
    virtual std::vector<uint8_t> serialize() const;

    // Type of the packet
    const Type type;

   protected:
    explicit Common(const packet::Type type) : type{type} {};

    // Used for performance reasons to avoid reallocation when serializing
    virtual size_t serialized_size() const { return sizeof(MAGIC) + sizeof(type); }
};

class StillAlive : public Common {
   public:
    StillAlive() : Common(Type::STILL_ALIVE) {}
};

class AnyoneThere : public Common {
   public:
    AnyoneThere() : Common(Type::ANYONE_THERE) {}
};

class IAmHere : public Common {
   public:
    IAmHere() : Common(Type::I_AM_HERE) {}
};

class PlsConnect : public Common {
   public:
    PlsConnect() : Common(Type::PLS_CONNECT) {}
};

class Welcome : public Common {
   public:
    Welcome() : Common(Type::WELCOME) {}
};

class NodeConnected : public Common {
   public:
    explicit NodeConnected(const MAC_ADDR& node) : Common(Type::NODE_CONNECTED), node{node} {}

    std::vector<uint8_t> serialize() const override;

    const MAC_ADDR& node;

   private:
    size_t serialized_size() const override { return Common::serialized_size() + sizeof(node); }
};

class NodeDisconnected : public Common {
   public:
    explicit NodeDisconnected(const MAC_ADDR& node) : Common(Type::NODE_DISCONNECTED), node{node} {}

    std::vector<uint8_t> serialize() const override;

    const MAC_ADDR& node;

   private:
    size_t serialized_size() const override { return Common::serialized_size() + sizeof(node); }
};

class MeshUnreachable : public Common {
   public:
    MeshUnreachable() : Common(Type::MESH_UNREACHABLE) {}
};

class MeshReachable : public Common {
   public:
    MeshReachable() : Common(Type::MESH_REACHABLE) {}
};

class Directed : public Common {
   public:
    const MAC_ADDR& target;
    const uint16_t seq_num;

   protected:
    Directed(const packet::Type type, const MAC_ADDR& target, const uint16_t seq_num)
        : Common(type), target{target}, seq_num{seq_num} {
        assert(seq_num <= MAX_SEQ_NUM);
    }

    std::vector<uint8_t> serialize() const override;

    size_t serialized_size() const override { return Common::serialized_size() + sizeof(target) + sizeof(seq_num); }
};

class DataAck : public Directed {
   public:
    DataAck(const MAC_ADDR& target, uint16_t seq_num) : Directed(Type::DATA_ACK, target, seq_num) {}
};

class DataNack : public Directed {
   public:
    DataNack(const MAC_ADDR& target, uint16_t seq_num) : Directed(Type::DATA_NACK, target, seq_num) {}
};

class DataCommon : public Directed {
   public:
    std::vector<uint8_t> serialize() const override;

    bool first;
    const uint16_t len_or_frag_num;
    const std::vector<uint8_t>& data;

   protected:
    DataCommon(packet::Type type, const MAC_ADDR& target, uint16_t seq_num, bool first, uint16_t len_or_frag_num,
               std::vector<uint8_t>& data)
        : Directed(type, target, seq_num), first{first}, len_or_frag_num{len_or_frag_num}, data{data} {
        if (first) {
            assert(len_or_frag_num >= 1 && len_or_frag_num <= MAX_DATA_TOTAL_SIZE);
        } else {
            assert(len_or_frag_num >= 1 && len_or_frag_num <= MAX_FRAG_NUM);
        }
        assert(!data.empty());
        if (first) {
            assert(data.size() <= MAX_DATA_FIRST_SIZE);
        } else {
            assert(data.size() <= MAX_DATA_NEXT_SIZE);
        }
    }

    size_t serialized_size() const override {
        return Directed::serialized_size() + sizeof(len_or_frag_num) + data.size();
    }
};

class DataLwIP : public DataCommon {
   public:
    DataLwIP(MAC_ADDR& target, uint16_t seq_num, bool first, uint16_t len_or_frag_num, std::vector<uint8_t>& data)
        : DataCommon(Type::DATA_LWIP, target, seq_num, first, len_or_frag_num, data) {}
};

class DataCustom : public DataCommon {
   public:
    DataCustom(const MAC_ADDR& target, uint16_t seq_num, bool first, uint16_t len_or_frag_num,
               std::vector<uint8_t>& data)
        : DataCommon(Type::DATA_CUSTOM, target, seq_num, first, len_or_frag_num, data) {}
};

}  // namespace packet
}  // namespace meshnow
