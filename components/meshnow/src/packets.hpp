#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "state.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::packets {

struct Status {
    state::State state;
    std::optional<util::MacAddr> root_mac;
};

struct SearchProbe {};

struct SearchReply {};

struct ConnectRequest {};

struct ConnectOk {
    util::MacAddr root_mac;
};

struct ResetRequest {
    uint32_t id;
    util::MacAddr mac;
};

struct ResetOk {
    uint32_t id;
    util::MacAddr root_mac;
};

struct RemoveFromRoutingTable {
    util::MacAddr mac;
};

struct RootUnreachable {};

struct RootReachable {
    util::MacAddr root_mac;
};

struct DataFragment {
    util::MacAddr source;
    util::MacAddr target;
    uint32_t id;
    uint8_t frag_num;
    uint16_t total_size;
    util::Buffer data;
};

using Payload = std::variant<Status, SearchProbe, SearchReply, ConnectRequest, ConnectOk, ResetRequest, ResetOk,
                             RemoveFromRoutingTable, RootUnreachable, RootReachable, DataFragment>;

struct Packet {
    uint32_t seq_num;
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
