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

std::vector<uint8_t> Common::serialize() const {
    std::vector<uint8_t> packet;
    packet.reserve(serialized_size());
    packet.insert(packet.end(), MAGIC.begin(), MAGIC.end());
    packet.push_back(static_cast<uint8_t>(type));
    return packet;
}

std::vector<uint8_t> NodeConnected::serialize() const {
    auto packet = Common::serialize();
    packet.insert(packet.end(), node.begin(), node.end());
    return packet;
}

std::vector<uint8_t> NodeDisconnected::serialize() const {
    auto packet = Common::serialize();
    packet.insert(packet.end(), node.begin(), node.end());
    return packet;
}

std::vector<uint8_t> Directed::serialize() const {
    auto packet = Common::serialize();
    packet.insert(packet.end(), target.begin(), target.end());
    auto seq_num_begin = reinterpret_cast<const uint8_t*>(&seq_num);
    packet.insert(packet.end(), seq_num_begin, seq_num_begin + sizeof(seq_num));
    return packet;
}

std::vector<uint8_t> DataCommon::serialize() const {
    auto packet = Directed::serialize();

    // construct new sequence number + (length or fragment number)
    // and replace the sequence number currently at the end
    // -> more compact

    // this will occupy one byte less if it's not the first fragment

    if (first) {
        struct __attribute__((packed)) seq_len {
            uint16_t seq : 13;
            uint16_t len : 11;
        };
        seq_len seq_len{seq_num, len_or_frag_num};
        auto seq_len_ptr = reinterpret_cast<uint8_t*>(&seq_len);
        packet.insert(packet.end(), seq_len_ptr, seq_len_ptr + sizeof(seq_len));
    } else {
        struct __attribute__((packed)) seq_frag_num {
            uint16_t seq : 13;
            uint8_t frag_num : 3;
        };
        seq_frag_num seq_frag_num{seq_num, static_cast<uint8_t>(len_or_frag_num)};
        auto seq_frag_num_ptr = reinterpret_cast<uint8_t*>(&seq_frag_num);
        packet.insert(packet.end(), seq_frag_num_ptr, seq_frag_num_ptr + sizeof(seq_frag_num));
    }
    packet.insert(packet.end(), data.begin(), data.end());
    return packet;
}

}  // namespace packet
}  // namespace meshnow
