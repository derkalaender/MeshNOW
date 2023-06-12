#include "packets.hpp"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/vector.h>

#include <optional>
#include <variant>
#include <vector>

#include "constants.hpp"

using OutputAdapter = bitsery::OutputBufferAdapter<meshnow::Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<meshnow::Buffer>;

// payload serialization
// need the namespace for bitsery to find the serialize function
namespace meshnow::packets {

template <typename S>
static void serialize(S&, meshnow::packets::KeepAlive&) {
    // no data
}

template <typename S>
static void serialize(S&, meshnow::packets::AnyoneThere&) {
    // no data
}

template <typename S>
static void serialize(S&, meshnow::packets::IAmHere&) {
    // no data
}

template <typename S>
static void serialize(S&, meshnow::packets::PlsConnect&) {
    // no data
}

template <typename S>
static void serialize(S& s, meshnow::packets::Verdict& p) {
    s.container1b(p.root_mac);
    s.boolValue(p.accept);
}

template <typename S>
static void serialize(S& s, meshnow::packets::NodeConnected& p) {
    s.container1b(p.parent_mac);
    s.container1b(p.child_mac);
}

template <typename S>
static void serialize(S& s, meshnow::packets::NodeDisconnected& p) {
    s.container1b(p.child_mac);
}

template <typename S>
static void serialize(S&, meshnow::packets::RootUnreachable&) {
    // no data
}

template <typename S>
static void serialize(S&, meshnow::packets::RootReachable&) {
    // no data
}

template <typename S>
static void serialize(S& s, meshnow::packets::Ack& p) {
    s.container1b(p.target);
    s.value4b(p.id_ack);
}

template <typename S>
static void serialize(S& s, meshnow::packets::Nack& p) {
    s.container1b(p.target);
    s.value4b(p.id_nack);
    s.value1b(p.reason);
}

template <typename S>
static void serialize(S& s, meshnow::packets::LwipDataFirst& p) {
    s.container1b(p.source);
    s.container1b(p.target);
    s.value4b(p.id);
    s.value2b(p.size);
    s.container1b(p.data, MAX_DATA_FIRST_SIZE);
}

template <typename S>
static void serialize(S& s, meshnow::packets::CustomDataFirst& p) {
    s.container1b(p.source);
    s.container1b(p.target);
    s.value4b(p.id);
    s.value2b(p.size);
    s.container1b(p.data, MAX_DATA_FIRST_SIZE);
}

template <typename S>
static void serialize(S& s, meshnow::packets::LwipDataNext& p) {
    s.container1b(p.source);
    s.container1b(p.target);
    s.value4b(p.id);
    s.value1b(p.frag_num);
    s.container1b(p.data, MAX_DATA_NEXT_SIZE);
}

template <typename S>
static void serialize(S& s, meshnow::packets::CustomDataNext& p) {
    s.container1b(p.source);
    s.container1b(p.target);
    s.value4b(p.id);
    s.value1b(p.frag_num);
    s.container1b(p.data, MAX_DATA_NEXT_SIZE);
}

}  // namespace meshnow::packets

// common header for every packet
struct Header {
    std::array<uint8_t, 3> magic;
    meshnow::packets::Type type;
    uint32_t id;
};

// serialize header
template <typename S>
static void serialize(S& s, Header& h) {
    s.container1b(h.magic);
    s.value1b(h.type);
    s.value4b(h.id);
}

meshnow::Buffer meshnow::packets::serialize(const meshnow::packets::Packet& packet) {
    Buffer buffer;
    buffer.reserve(sizeof(meshnow::packets::Packet));

    // write header
    Header header{meshnow::MAGIC, getType(packet.payload), packet.id};
    auto written_header_size = bitsery::quickSerialization(OutputAdapter{buffer}, header);

    // create out adapter that is advanced past the header so that we don't overwrite it with the payload
    OutputAdapter out{buffer};
    out.currentWritePos(written_header_size);
    // write payload
    auto written_size =
        std::visit([&out](auto& p) { return bitsery::quickSerialization(std::move(out), p); }, packet.payload);

    // shrink and return buffer
    // written size includes the header size already
    buffer.resize(written_size);
    return buffer;
}

// helper function to avoid repetition...
// deserializes the specific payload and writes it into the given optional
template <typename P, typename It>
static inline void deserializePayload(std::optional<meshnow::packets::Payload>& opt, It buffer, size_t size) {
    P payload{};
    auto state = bitsery::quickDeserialization(InputAdapter{buffer, size}, payload);
    // we except to read the whole buffer
    if (state.first == bitsery::ReaderError::NoError && state.second) {
        // put into optional
        opt.emplace(std::move(payload));
    }
}

std::optional<meshnow::packets::Packet> meshnow::packets::deserialize(const meshnow::Buffer& buffer) {
    // read header
    Header header{};
    auto state = bitsery::quickDeserialization(InputAdapter{buffer.begin(), meshnow::HEADER_SIZE}, header);
    // check if deserialization was successful
    // don't check if we read the whole buffer, because the payload is still in there
    if (state.first != bitsery::ReaderError::NoError) {
        return std::nullopt;
    }

    // check magic
    if (header.magic != meshnow::MAGIC) {
        return std::nullopt;
    }

    // check type
    if (header.type >= meshnow::packets::Type::MAX) {
        return std::nullopt;
    }

    // deserialize payload past the header
    auto it = buffer.begin() + meshnow::HEADER_SIZE;
    auto size = buffer.size() - meshnow::HEADER_SIZE;
    std::optional<meshnow::packets::Payload> payload;

    // select corresponding deserializer
    switch (header.type) {
        case Type::KEEP_ALIVE:
            deserializePayload<meshnow::packets::KeepAlive>(payload, it, size);
            break;
        case Type::ANYONE_THERE:
            deserializePayload<meshnow::packets::AnyoneThere>(payload, it, size);
            break;
        case Type::I_AM_HERE:
            deserializePayload<meshnow::packets::IAmHere>(payload, it, size);
            break;
        case Type::PLS_CONNECT:
            deserializePayload<meshnow::packets::PlsConnect>(payload, it, size);
            break;
        case Type::VERDICT:
            deserializePayload<meshnow::packets::Verdict>(payload, it, size);
            break;
        case Type::NODE_CONNECTED:
            deserializePayload<meshnow::packets::NodeConnected>(payload, it, size);
            break;
        case Type::NODE_DISCONNECTED:
            deserializePayload<meshnow::packets::NodeDisconnected>(payload, it, size);
            break;
        case Type::ROOT_UNREACHABLE:
            deserializePayload<meshnow::packets::RootUnreachable>(payload, it, size);
            break;
        case Type::ROOT_REACHABLE:
            deserializePayload<meshnow::packets::RootReachable>(payload, it, size);
            break;
        case Type::DATA_ACK:
            deserializePayload<meshnow::packets::Ack>(payload, it, size);
            break;
        case Type::DATA_NACK:
            deserializePayload<meshnow::packets::Nack>(payload, it, size);
            break;
        case Type::DATA_LWIP_FIRST:
            deserializePayload<meshnow::packets::LwipDataFirst>(payload, it, size);
            break;
        case Type::DATA_LWIP_NEXT:
            deserializePayload<meshnow::packets::LwipDataNext>(payload, it, size);
            break;
        case Type::DATA_CUSTOM_FIRST:
            deserializePayload<meshnow::packets::CustomDataFirst>(payload, it, size);
            break;
        case Type::DATA_CUSTOM_NEXT:
            deserializePayload<meshnow::packets::CustomDataNext>(payload, it, size);
            break;
        case Type::MAX:
            // nop
            break;
    }

    // wrap payload in packet
    if (payload) {
        return meshnow::packets::Packet{header.id, *payload};
    } else {
        return std::nullopt;
    }
}

meshnow::packets::Type meshnow::packets::getType(const meshnow::packets::Payload& payload) {
    // return the static constexpr type of the payload
    return std::visit([](auto& p) { return p.type; }, payload);
}