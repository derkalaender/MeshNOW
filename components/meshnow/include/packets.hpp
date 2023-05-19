#pragma once

#include <cstdint>
#include <optional>
#include <variant>
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

struct StillAlive {
    static constexpr Type type{Type::STILL_ALIVE};
};

struct AnyoneThere {
    static constexpr Type type{Type::ANYONE_THERE};
};

struct IAmHere {
    static constexpr Type type{Type::I_AM_HERE};
};

struct PlsConnect {
    static constexpr Type type{Type::PLS_CONNECT};
};

struct Verdict {
    static constexpr Type type{Type::VERDICT};

    meshnow::MAC_ADDR root_mac;
    bool accept;
};

struct NodeConnected {
    static constexpr Type type{Type::NODE_CONNECTED};

    MAC_ADDR child_mac;
};

struct NodeDisconnected {
    static constexpr Type type{Type::NODE_DISCONNECTED};

    MAC_ADDR child_mac;
};

struct MeshUnreachable {
    static constexpr Type type{Type::MESH_UNREACHABLE};
};

struct MeshReachable {
    static constexpr Type type{Type::MESH_REACHABLE};
};

struct Ack {
    static constexpr Type type{Type::DATA_ACK};

    uint16_t seq_num_ack;
};

struct Nack {
    static constexpr Type type{Type::DATA_NACK};

    uint16_t seq_num_nack;
};

template <bool is_lwip>
struct DataFirst {
    static constexpr Type type{is_lwip ? Type::DATA_LWIP_FIRST : Type::DATA_CUSTOM_FIRST};

    meshnow::MAC_ADDR target;
    uint16_t size;
    meshnow::Buffer data;
};

using LwipDataFirst = DataFirst<true>;
using CustomDataFirst = DataFirst<false>;

template <bool is_lwip>
struct DataNext {
    static constexpr Type type{is_lwip ? Type::DATA_LWIP_NEXT : Type::DATA_CUSTOM_NEXT};

    meshnow::MAC_ADDR target;
    uint8_t frag_num;
    meshnow::Buffer data;
};

using LwipDataNext = DataNext<true>;
using CustomDataNext = DataNext<false>;

using Payload = std::variant<StillAlive, AnyoneThere, IAmHere, PlsConnect, Verdict, NodeConnected, NodeDisconnected,
                             MeshUnreachable, MeshReachable, Ack, Nack, LwipDataFirst, CustomDataFirst, LwipDataNext,
                             CustomDataNext>;

struct Packet {
    uint16_t seq_num;
    Payload payload;
};

/**
 * Serialize the given packet into a byte buffer
 * @param packet The packet to serialize
 * @return The serialized packet as a byte buffer
 */
meshnow::Buffer serialize(const Packet& packet);

/**
 * Deserialize the given byte buffer into a packet
 * @param buffer The byte buffer to deserialize
 * @return The deserialized packet. If the buffer is invalid, std::nullopt is returned
 */
std::optional<Packet> deserialize(const meshnow::Buffer& buffer);

/**
 * Get the type of the given payload
 * @param packet The payload to get the type of
 * @return The type of the payload
 */
Type getType(const Payload& payload);

}  // namespace meshnow::packets
