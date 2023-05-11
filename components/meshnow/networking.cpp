#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "internal.hpp"

static const char* TAG = CREATE_TAG("Networking");

// TODO handle list of peers full
static void add_peer(const meshnow::MAC_ADDR& mac_addr) {
    if (esp_now_is_peer_exist(mac_addr.data())) {
        return;
    }
    esp_now_peer_info_t peer_info{};
    peer_info.channel = 0;
    peer_info.encrypt = false;
    peer_info.ifidx = WIFI_IF_STA;
    std::copy(mac_addr.begin(), mac_addr.end(), peer_info.peer_addr);
    CHECK_THROW(esp_now_add_peer(&peer_info));
}

namespace meshnow {

void Networking::raw_broadcast(const std::vector<uint8_t>& payload) { raw_send(BROADCAST_MAC_ADDR, payload); }
void Networking::raw_send(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& payload) {
    if (payload.size() > MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum payload size %d", payload.size(), MAX_RAW_PACKET_SIZE);
        throw PayloadTooLargeException();
    }

    add_peer(mac_addr);
    ESP_LOGI(TAG, "Sending raw payload to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), payload.data(), payload.size()));
}

void Networking::on_send(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // TODO
    ESP_LOGI(TAG, "Send status: %d", status);
}

void Networking::on_receive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    ESP_LOGI(TAG, "Received data");
    // TODO error checking and timeout

    RecvData recv_data{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), recv_data.src_addr.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), recv_data.dest_addr.begin());
    recv_data.rssi = esp_now_info->rx_ctrl->rssi;
    recv_data.data = std::vector<uint8_t>(data, data + data_len);
    receive_queue.push_back(std::move(recv_data), portMAX_DELAY);
}

[[noreturn]] void Networking::ReceiveWorker() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto recv_data = receive_queue.pop(portMAX_DELAY);
        if (!recv_data) continue;

        ESP_LOGI(TAG, "Received data from " MAC_FORMAT, MAC_FORMAT_ARGS(recv_data->src_addr));
        ESP_LOGI(TAG, "SIZE: %d", recv_data->data.size());
        ESP_LOG_BUFFER_HEXDUMP(TAG, recv_data->data.data(), recv_data->data.size(), ESP_LOG_INFO);
    }
}

namespace packet {

// Helper structs for data serialization to not waste bytes //

struct __attribute__((packed)) seq_len {
    uint16_t seq : 13;
    uint16_t len : 11;
};

struct __attribute__((packed)) seq_frag {
    uint16_t seq : 13;
    uint8_t frag_num : 3;
};

// Packet (de)serialization //

std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(payload_.serializedSize() + sizeof(MAGIC) + sizeof(Type));
    buffer.insert(buffer.end(), MAGIC.begin(), MAGIC.end());
    buffer.push_back(static_cast<uint8_t>(payload_.type()));
    payload_.serialize(buffer);
    return buffer;
}

std::unique_ptr<BasePayload> Packet::deserialize(const std::vector<uint8_t>& buffer) {
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
    Type type = static_cast<Type>(*it);
    if (type >= Type::MAX) {
        return nullptr;
    }

    it += sizeof(Type);

    size_t payload_size{buffer.size() - sizeof(MAGIC) - sizeof(Type)};

    // deserialize
    // TODO use less copying. maybe iterator?
    switch (type) {
        case Type::STILL_ALIVE:
            if (payload_size != 0) break;
            return std::make_unique<StillAlivePayload>();
        case Type::ANYONE_THERE:
            if (payload_size != 0) break;
            return std::make_unique<AnyoneTherePayload>();
        case Type::I_AM_HERE:
            if (payload_size != 0) break;
            return std::make_unique<IAmHerePayload>();
        case Type::PLS_CONNECT:
            if (payload_size != 0) break;
            return std::make_unique<PlsConnectPayload>();
        case Type::WELCOME:
            if (payload_size != 0) break;
            return std::make_unique<WelcomePayload>();
        case Type::NODE_CONNECTED: {
            if (payload_size != sizeof(MAC_ADDR)) break;
            MAC_ADDR addr;
            std::copy(it, it + sizeof(MAC_ADDR), addr.begin());
            return std::make_unique<NodeConnectedPayload>(addr);
        }
        case Type::NODE_DISCONNECTED: {
            if (payload_size != sizeof(MAC_ADDR)) break;
            MAC_ADDR addr;
            std::copy(it, it + sizeof(MAC_ADDR), addr.begin());
            return std::make_unique<NodeDisconnectedPayload>(addr);
        }
        case Type::MESH_UNREACHABLE:
            if (payload_size != 0) break;
            return std::make_unique<MeshUnreachablePayload>();
        case Type::MESH_REACHABLE:
            if (payload_size != 0) break;
            return std::make_unique<MeshReachablePayload>();
        case Type::DATA_ACK: {
            // mac, seq num
            if (payload_size != sizeof(MAC_ADDR) + sizeof(uint16_t)) break;
            MAC_ADDR addr;
            std::copy(it, it + sizeof(MAC_ADDR), addr.begin());
            it += sizeof(MAC_ADDR);
            uint16_t seq_num = *reinterpret_cast<const uint16_t*>(&*it);
            return std::make_unique<DataAckPayload>(addr, seq_num);
        }
        case Type::DATA_NACK: {
            // mac, seq num
            if (payload_size != sizeof(MAC_ADDR) + sizeof(uint16_t)) break;
            MAC_ADDR addr;
            std::copy(it, it + sizeof(MAC_ADDR), addr.begin());
            it += sizeof(MAC_ADDR);
            uint16_t seq_num = *reinterpret_cast<const uint16_t*>(&*it);
            return std::make_unique<DataNackPayload>(addr, seq_num);
        }
        case Type::DATA_LWIP_FIRST:
        case Type::DATA_CUSTOM_FIRST: {
            // mac, seq num + len + min data size
            if (payload_size < sizeof(MAC_ADDR) + sizeof(seq_len) + 1) break;
            MAC_ADDR addr;
            std::copy(it, it + sizeof(MAC_ADDR), addr.begin());
            it += sizeof(MAC_ADDR);
            seq_len sl = *reinterpret_cast<const seq_len*>(&*it);
            uint16_t seq_num = sl.seq;
            uint16_t len = sl.len;
            // sanity checks because packet could be malicious
            if (!(len >= 1 && len <= MAX_DATA_TOTAL_SIZE)) break;
            // TODO check seq_num

            it += sizeof(seq_len);
            std::vector<uint8_t> data{it, buffer.end()};
            return std::make_unique<DataFirstPayload>(addr, seq_num, len, type == Type::DATA_CUSTOM_FIRST, data);
        }
        case Type::DATA_LWIP_NEXT:
        case Type::DATA_CUSTOM_NEXT: {
            // mac, seq num + frag num + min data size
            if (payload_size < sizeof(MAC_ADDR) + sizeof(seq_frag) + 1) {
                ESP_LOGE(TAG, "Payload size %d too small for next data packet", payload_size);
                break;
            }
            MAC_ADDR addr;
            std::copy(it, it + sizeof(MAC_ADDR), addr.begin());
            it += sizeof(MAC_ADDR);
            seq_frag sf = *reinterpret_cast<const seq_frag*>(&*it);
            uint16_t seq_num = sf.seq;
            uint8_t frag_num = sf.frag_num;
            // sanity checks because packet could be malicious
            if (!(frag_num >= 1 && frag_num < MAX_FRAG_NUM)) break;
            // TODO check seq_num

            it += sizeof(seq_frag);
            std::vector<uint8_t> data{it, buffer.end()};
            return std::make_unique<DataNextPayload>(addr, seq_num, frag_num, type == Type::DATA_CUSTOM_NEXT, data);
        }
        case Type::MAX:
            // ignore
            break;
    }

    // null per default. could also use optional to be more explicit but who cares
    return nullptr;
}

void NodeConnectedPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), connected_to_.begin(), connected_to_.end());
}

size_t NodeConnectedPayload::serializedSize() const { return sizeof(connected_to_); }

void NodeDisconnectedPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), disconnected_from_.begin(), disconnected_from_.end());
}

size_t NodeDisconnectedPayload::serializedSize() const { return sizeof(disconnected_from_); }

void DataAckPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());
    auto seq_num_begin = reinterpret_cast<const uint8_t*>(&seq_num_);
    buffer.insert(buffer.end(), seq_num_begin, seq_num_begin + sizeof(seq_num_));
}

size_t DataAckPayload::serializedSize() const { return sizeof(target_) + sizeof(seq_num_); }

void DataNackPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());
    auto seq_num_begin = reinterpret_cast<const uint8_t*>(&seq_num_);
    buffer.insert(buffer.end(), seq_num_begin, seq_num_begin + sizeof(seq_num_));
}

size_t DataNackPayload::serializedSize() const { return sizeof(target_) + sizeof(seq_num_); }

void DataFirstPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());

    seq_len sl{seq_num_, len_};
    auto sl_ptr = reinterpret_cast<uint8_t*>(&sl);
    buffer.insert(buffer.end(), sl_ptr, sl_ptr + sizeof(sl));

    // append user payload
    buffer.insert(buffer.end(), data_.begin(), data_.end());
}

size_t DataFirstPayload::serializedSize() const { return data_.size() + sizeof(target_) + sizeof(seq_len); }

void DataNextPayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());

    seq_frag sf{seq_num_, frag_num_};
    auto sf_ptr = reinterpret_cast<uint8_t*>(&sf);
    buffer.insert(buffer.end(), sf_ptr, sf_ptr + sizeof(sf));

    // append user payload
    buffer.insert(buffer.end(), data_.begin(), data_.end());
}

size_t DataNextPayload::serializedSize() const { return data_.size() + sizeof(target_) + sizeof(seq_frag); }

}  // namespace packet
}  // namespace meshnow
