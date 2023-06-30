#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::packets {

struct KeepAlive {};

struct AnyoneThere {};

struct IAmHere {};

struct PlsConnect {};

struct Verdict {
    util::MacAddr root_mac;
    bool accept;
};

struct NodeConnected {
    util::MacAddr parent_mac;
    util::MacAddr child_mac;
};

struct NodeDisconnected {
    util::MacAddr child_mac;
};

struct RootUnreachable {};

struct RootReachable {};

struct DataFragment {
    util::MacAddr source;
    util::MacAddr target;
    uint32_t id;
    uint8_t frag_num;
    uint16_t total_size;
    util::Buffer data;
};

using Payload = std::variant<KeepAlive, AnyoneThere, IAmHere, PlsConnect, Verdict, NodeConnected, NodeDisconnected,
                             RootUnreachable, RootReachable, DataFragment>;

struct Packet {
    uint32_t id;
    Payload payload;
};

/**
 * Serialize the given packet into a byte buffer
 * @param packet The packet to serialize
 * @return The serialized packet as a byte buffer
 */
util::Buffer serialize(const Packet& packet);

/**
 * Deserialize the given byte buffer into a packet
 * @param buffer The byte buffer to deserialize
 * @return The deserialized packet. If the buffer is invalid, std::nullopt is returned
 */
std::optional<Packet> deserialize(const util::Buffer& buffer);

}  // namespace meshnow::packets