#include "packets.hpp"

#include <vector>

#include "constants.hpp"
#include "networking.hpp"

// Helper structs for data serialization to not waste bytes //

struct __attribute__((packed)) seq_len {
    uint16_t seq : 13;
    uint16_t len : 11;
};

struct __attribute__((packed)) seq_frag {
    uint16_t seq : 13;
    uint8_t frag_num : 3;
};

// Actual (de)serialization //

std::vector<uint8_t> meshnow::packets::Packet::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(payload_.serializedSize() + sizeof(MAGIC) + sizeof(Type));
    buffer.insert(buffer.end(), MAGIC.begin(), MAGIC.end());
    buffer.push_back(static_cast<uint8_t>(payload_.type()));
    payload_.serialize(buffer);
    return buffer;
}

std::unique_ptr<meshnow::packets::BasePayload> meshnow::packets::Packet::deserialize(
    const std::vector<uint8_t>& buffer) {
    // sanity checks

    // check minimum packet size
    if (buffer.size() < sizeof(MAGIC) + sizeof(Type)) {
        return nullptr;
    }

    // iterator for easy traversal
    auto it = buffer.begin();

    // check magic
    if (!std::equal(it, it + sizeof(MAGIC), MAGIC.begin())) {
        return nullptr;
    }

    it += sizeof(MAGIC);

    // check type
    auto type = static_cast<meshnow::packets::Type>(*it);
    if (type >= meshnow::packets::Type::MAX) {
        return nullptr;
    }

    it += sizeof(meshnow::packets::Type);

    size_t payload_size{buffer.size() - sizeof(MAGIC) - sizeof(Type)};

    // deserialize
    // TODO use less copying. maybe iterator?
    switch (type) {
        case meshnow::packets::Type::STILL_ALIVE:
            if (payload_size != 0) break;
            return std::make_unique<meshnow::packets::StillAlivePayload>();
        case meshnow::packets::Type::ANYONE_THERE:
            if (payload_size != 0) break;
            return std::make_unique<meshnow::packets::AnyoneTherePayload>();
        case Type::I_AM_HERE:
            if (payload_size != 0) break;
            return std::make_unique<IAmHerePayload>();
        case meshnow::packets::Type::PLS_CONNECT:
            if (payload_size != 0) break;
            return std::make_unique<meshnow::packets::PlsConnectPayload>();
        case Type::VERDICT: {
            if (payload_size != sizeof(bool)) break;
            bool accept_connection = *it;
            it += sizeof(bool);
            meshnow::MAC_ADDR root_mac = *reinterpret_cast<const meshnow::MAC_ADDR*>(&*it);
            return std::make_unique<VerdictPayload>(accept_connection, root_mac);
        }
        case meshnow::packets::Type::NODE_CONNECTED: {
            if (payload_size != sizeof(meshnow::MAC_ADDR)) break;
            meshnow::MAC_ADDR addr;
            std::copy(it, it + sizeof(meshnow::MAC_ADDR), addr.begin());
            return std::make_unique<meshnow::packets::NodeConnectedPayload>(addr);
        }
        case Type::NODE_DISCONNECTED: {
            if (payload_size != sizeof(meshnow::MAC_ADDR)) break;
            meshnow::MAC_ADDR addr;
            std::copy(it, it + sizeof(meshnow::MAC_ADDR), addr.begin());
            return std::make_unique<meshnow::packets::NodeDisconnectedPayload>(addr);
        }
        case meshnow::packets::Type::MESH_UNREACHABLE:
            if (payload_size != 0) break;
            return std::make_unique<meshnow::packets::MeshUnreachablePayload>();
        case Type::MESH_REACHABLE:
            if (payload_size != 0) break;
            return std::make_unique<MeshReachablePayload>();
        case meshnow::packets::Type::DATA_ACK: {
            // mac, seq num
            if (payload_size != sizeof(meshnow::MAC_ADDR) + sizeof(uint16_t)) break;
            meshnow::MAC_ADDR addr;
            std::copy(it, it + sizeof(meshnow::MAC_ADDR), addr.begin());
            it += sizeof(meshnow::MAC_ADDR);
            uint16_t seq_num = *reinterpret_cast<const uint16_t*>(&*it);
            return std::make_unique<DataAckPayload>(addr, seq_num);
        }
        case meshnow::packets::Type::DATA_NACK: {
            // mac, seq num
            if (payload_size != sizeof(meshnow::MAC_ADDR) + sizeof(uint16_t)) break;
            meshnow::MAC_ADDR addr;
            std::copy(it, it + sizeof(meshnow::MAC_ADDR), addr.begin());
            it += sizeof(meshnow::MAC_ADDR);
            uint16_t seq_num = *reinterpret_cast<const uint16_t*>(&*it);
            return std::make_unique<DataNackPayload>(addr, seq_num);
        }
        case meshnow::packets::Type::DATA_LWIP_FIRST:
        case Type::DATA_CUSTOM_FIRST: {
            // mac, seq num + len + min data size
            if (payload_size < sizeof(meshnow::MAC_ADDR) + sizeof(seq_len) + 1) break;
            meshnow::MAC_ADDR addr;
            std::copy(it, it + sizeof(meshnow::MAC_ADDR), addr.begin());
            it += sizeof(meshnow::MAC_ADDR);
            seq_len sl = *reinterpret_cast<const seq_len*>(&*it);
            uint16_t seq_num = sl.seq;
            uint16_t len = sl.len;
            // sanity checks because packet could be malicious
            if (!(len >= 1 && len <= meshnow::MAX_DATA_TOTAL_SIZE)) break;
            // TODO check seq_num

            it += sizeof(seq_len);
            std::vector<uint8_t> data{it, buffer.end()};
            return std::make_unique<meshnow::packets::DataFirstPayload>(
                addr, seq_num, len, type == meshnow::packets::Type::DATA_CUSTOM_FIRST, data);
        }
        case meshnow::packets::Type::DATA_LWIP_NEXT:
        case meshnow::packets::Type::DATA_CUSTOM_NEXT: {
            // mac, seq num + frag num + min data size
            if (payload_size < sizeof(meshnow::MAC_ADDR) + sizeof(seq_frag) + 1) break;
            meshnow::MAC_ADDR addr;
            std::copy(it, it + sizeof(meshnow::MAC_ADDR), addr.begin());
            it += sizeof(meshnow::MAC_ADDR);
            seq_frag sf = *reinterpret_cast<const seq_frag*>(&*it);
            uint16_t seq_num = sf.seq;
            uint8_t frag_num = sf.frag_num;
            // sanity checks because packet could be malicious
            if (!(frag_num >= 1 && frag_num < meshnow::MAX_FRAG_NUM)) break;
            // TODO check seq_num

            it += sizeof(seq_frag);
            std::vector<uint8_t> data{it, buffer.end()};
            return std::make_unique<DataNextPayload>(addr, seq_num, frag_num, type == Type::DATA_CUSTOM_NEXT, data);
        }
        case meshnow::packets::Type::MAX:
            // ignore
            break;
    }

    // null per default. could also use optional to be more explicit but who cares
    return nullptr;
}

void meshnow::packets::VerdictPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.push_back(accept_connection_);
}

size_t meshnow::packets::VerdictPayload::serializedSize() const { return sizeof(accept_connection_); }

void meshnow::packets::NodeConnectedPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), connected_to_.begin(), connected_to_.end());
}

size_t meshnow::packets::NodeConnectedPayload::serializedSize() const { return sizeof(connected_to_); }

void meshnow::packets::NodeDisconnectedPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), disconnected_from_.begin(), disconnected_from_.end());
}

size_t meshnow::packets::NodeDisconnectedPayload::serializedSize() const { return sizeof(disconnected_from_); }

void meshnow::packets::DataAckPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());
    auto seq_num_begin = reinterpret_cast<const uint8_t*>(&seq_num_);
    buffer.insert(buffer.end(), seq_num_begin, seq_num_begin + sizeof(seq_num_));
}

size_t meshnow::packets::DataAckPayload::serializedSize() const { return sizeof(target_) + sizeof(seq_num_); }

void meshnow::packets::DataNackPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());
    auto seq_num_begin = reinterpret_cast<const uint8_t*>(&seq_num_);
    buffer.insert(buffer.end(), seq_num_begin, seq_num_begin + sizeof(seq_num_));
}

size_t meshnow::packets::DataNackPayload::serializedSize() const { return sizeof(target_) + sizeof(seq_num_); }

void meshnow::packets::DataFirstPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());

    seq_len sl{seq_num_, len_};
    auto sl_ptr = reinterpret_cast<uint8_t*>(&sl);
    buffer.insert(buffer.end(), sl_ptr, sl_ptr + sizeof(sl));

    // append user payload
    buffer.insert(buffer.end(), data_.begin(), data_.end());
}

size_t meshnow::packets::DataFirstPayload::serializedSize() const {
    return data_.size() + sizeof(target_) + sizeof(seq_len);
}

void meshnow::packets::DataNextPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());

    seq_frag sf{seq_num_, frag_num_};
    auto sf_ptr = reinterpret_cast<uint8_t*>(&sf);
    buffer.insert(buffer.end(), sf_ptr, sf_ptr + sizeof(sf));

    // append user payload
    buffer.insert(buffer.end(), data_.begin(), data_.end());
}

size_t meshnow::packets::DataNextPayload::serializedSize() const {
    return data_.size() + sizeof(target_) + sizeof(seq_frag);
}

// Visitor handlers //

void meshnow::packets::StillAlivePayload::handle(meshnow::Networking& networking,
                                                 const meshnow::ReceiveMeta& meta) const {
    networking.handleStillAlive(meta);
}

void meshnow::packets::AnyoneTherePayload::handle(meshnow::Networking& networking,
                                                  const meshnow::ReceiveMeta& meta) const {
    networking.handleAnyoneThere(meta);
}

void meshnow::packets::IAmHerePayload::handle(meshnow::Networking& networking, const meshnow::ReceiveMeta& meta) const {
    networking.handleIAmHere(meta);
}

void meshnow::packets::PlsConnectPayload::handle(meshnow::Networking& networking,
                                                 const meshnow::ReceiveMeta& meta) const {
    networking.handlePlsConnect(meta);
}

void meshnow::packets::VerdictPayload::handle(meshnow::Networking& networking, const meshnow::ReceiveMeta& meta) const {
    networking.handleVerdict(meta, *this);
}

void meshnow::packets::NodeConnectedPayload::handle(meshnow::Networking& networking,
                                                    const meshnow::ReceiveMeta& meta) const {
    networking.handleNodeConnected(meta, *this);
}

void meshnow::packets::NodeDisconnectedPayload::handle(meshnow::Networking& networking,
                                                       const meshnow::ReceiveMeta& meta) const {
    networking.handleNodeDisconnected(meta, *this);
}

void meshnow::packets::MeshUnreachablePayload::handle(meshnow::Networking& networking,
                                                      const meshnow::ReceiveMeta& meta) const {
    networking.handleMeshUnreachable(meta);
}

void meshnow::packets::MeshReachablePayload::handle(meshnow::Networking& networking,
                                                    const meshnow::ReceiveMeta& meta) const {
    networking.handleMeshReachable(meta);
}

void meshnow::packets::DataAckPayload::handle(meshnow::Networking& networking, const meshnow::ReceiveMeta& meta) const {
    networking.handleDataAck(meta, *this);
}

void meshnow::packets::DataNackPayload::handle(meshnow::Networking& networking,
                                               const meshnow::ReceiveMeta& meta) const {
    networking.handleDataNack(meta, *this);
}

void meshnow::packets::DataFirstPayload::handle(meshnow::Networking& networking,
                                                const meshnow::ReceiveMeta& meta) const {
    networking.handleDataFirst(meta, *this);
}

void meshnow::packets::DataNextPayload::handle(meshnow::Networking& networking,
                                               const meshnow::ReceiveMeta& meta) const {
    networking.handleDataNext(meta, *this);
}
