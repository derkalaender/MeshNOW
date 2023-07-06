#include "packets.hpp"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/deserializer.h>
#include <bitsery/ext/std_optional.h>
#include <bitsery/ext/std_variant.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/vector.h>
#include <esp_now.h>

#include <optional>
#include <variant>
#include <vector>

using OutputAdapter = bitsery::OutputBufferAdapter<meshnow::util::Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<meshnow::util::Buffer>;

// common header for every packet
struct Header {
    std::array<uint8_t, 3> magic;
    uint32_t id;
};

// packet the way it is sent over ESP-NOW
struct WirePacket {
    Header header;
    meshnow::packets::Payload payload;
};

// CONSTANTS //
constexpr std::array<uint8_t, 3> MAGIC{0x55, 0x77, 0x55};
constexpr auto HEADER_SIZE{sizeof(Header)};
constexpr auto MAX_FRAG_PAYLOAD_SIZE{ESP_NOW_MAX_DATA_LEN - HEADER_SIZE - 19};

// PAYLOAD SERIALIZERS //

namespace meshnow::packets {

template <typename S>
static void serialize(S& s, Status& p) {
    s.value1b(p.state);
    // TODO optimize with custom extension
    s.ext(p.root, bitsery::ext::StdOptional{});
}

template <typename S>
static void serialize(S&, SearchProbe&) {
    // no data
}

template <typename S>
static void serialize(S&, SearchReply&) {
    // no data
}

template <typename S>
static void serialize(S&, ConnectRequest&) {
    // no data
}

template <typename S>
static void serialize(S& s, ConnectOk& p) {
    s.object(p.root);
}

template <typename S>
static void serialize(S& s, ResetRequest& p) {
    s.value4b(p.id);
    s.object(p.from);
}

template <typename S>
static void serialize(S& s, ResetOk& p) {
    s.value4b(p.id);
    s.object(p.to);
}

template <typename S>
static void serialize(S& s, RemoveFromRoutingTable& p) {
    s.object(p.to_remove);
}

template <typename S>
static void serialize(S&, RootUnreachable&) {
    // no data
}

template <typename S>
static void serialize(S& s, RootReachable& p) {
    s.object(p.root);
}

template <typename S>
static void serialize(S& s, DataFragment& p) {
    s.object(p.from);
    s.object(p.to);
    s.value4b(p.id);
    s.value1b(p.frag_num);
    s.value2b(p.total_size);
    // TODO optimize with custom extension
    s.container1b(p.data, MAX_FRAG_PAYLOAD_SIZE);
}

}  // namespace meshnow::packets

// HELPER SERIALIZERS //

namespace meshnow::util {

template <typename S>
static void serialize(S& s, MacAddr& mac) {
    s.container1b(mac.addr);
}

}  // namespace meshnow::util

template <typename S>
static void serialize(S& s, Header& h) {
    s.container1b(h.magic);
    s.value4b(h.id);
}

template <typename S>
static void serialize(S& s, WirePacket& wp) {
    s.object(wp.header);
    s.ext(wp.payload, bitsery::ext::StdVariant{[](S& s, auto& p) { s.object(p); }});
}

namespace meshnow::packets {

// PACKET SERIALIZATION //

util::Buffer serialize(const Packet& packet) {
    util::Buffer buffer;
    buffer.reserve(sizeof(WirePacket));

    WirePacket wp{.header = {.magic = MAGIC, .id = packet.seq}, .payload = packet.payload};

    // write
    auto written_size = bitsery::quickSerialization(OutputAdapter{buffer}, wp);

    // shrink and return
    buffer.resize(written_size);
    return buffer;
}

std::optional<Packet> deserialize(const util::Buffer& buffer) {
    WirePacket wp;

    // read
    auto [error, red_everything] = bitsery::quickDeserialization(InputAdapter{buffer.begin(), buffer.size()}, wp);

    // check for errors
    if (error == bitsery::ReaderError::NoError && red_everything) {
        return Packet{wp.header.id, wp.payload};
    } else {
        return std::nullopt;
    }
}

}  // namespace meshnow::packets