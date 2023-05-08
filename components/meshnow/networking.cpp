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
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + MAC_ADDR_LEN, recv_data.src_addr.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + MAC_ADDR_LEN, recv_data.dest_addr.begin());
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

std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(payload_.serializedSize() + sizeof(MAGIC) + sizeof(Type));
    buffer.insert(buffer.end(), MAGIC.begin(), MAGIC.end());
    buffer.push_back(static_cast<uint8_t>(payload_.type()));
    payload_.serialize(buffer);
    return buffer;
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

struct __attribute__((packed)) seq_len {
    uint16_t seq : 13;
    uint16_t len : 11;
};

struct __attribute__((packed)) seq_frag_num {
    uint16_t seq : 13;
    uint8_t frag_num : 3;
};

void DataBasePayload::serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), target_.begin(), target_.end());

    // sequence number + (length or fragment number) will be encoded
    // tightly packed to save space

    // this will occupy one byte less if it's not the first fragment

    if (first_) {
        seq_len seq_len{seq_num_, len_or_frag_num_};
        auto seq_len_ptr = reinterpret_cast<uint8_t*>(&seq_len);
        buffer.insert(buffer.end(), seq_len_ptr, seq_len_ptr + sizeof(seq_len));
    } else {
        seq_frag_num seq_frag_num{seq_num_, static_cast<uint8_t>(len_or_frag_num_)};
        auto seq_frag_num_ptr = reinterpret_cast<uint8_t*>(&seq_frag_num);
        buffer.insert(buffer.end(), seq_frag_num_ptr, seq_frag_num_ptr + sizeof(seq_frag_num));
    }

    // append user payload
    buffer.insert(buffer.end(), data_.begin(), data_.end());
}

size_t DataBasePayload::serializedSize() const {
    return data_.size() + (first_ ? sizeof(seq_len) : sizeof(seq_frag_num));
}

}  // namespace packet
}  // namespace meshnow
