#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "internal.hpp"

static const char* TAG = CREATE_TAG("Networking");

using namespace MeshNOW;

// TODO handle list of peers full
static void add_peer(const MAC_ADDR& mac_addr) {
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

std::vector<uint8_t> Packet::Common::serialize() const {
    std::vector<uint8_t> packet(MAGIC.begin(), MAGIC.end());
    packet.push_back(static_cast<uint8_t>(type));
    return packet;
}

std::vector<uint8_t> Packet::Directed::serialize() const {
    auto packet = Common::serialize();
    packet.insert(packet.end(), target.begin(), target.end());
    auto seq_num_begin = reinterpret_cast<const uint8_t*>(&seq_num);
    packet.insert(packet.end(), seq_num_begin, seq_num_begin + sizeof(seq_num));
    return packet;
}

std::vector<uint8_t> Packet::DataCommon::serialize() const {
    auto packet = Directed::serialize();

    // construct new sequence number + (length or fragment number)
    // and replace the sequence number currently at the end

    if (first) {
        struct __attribute__((packed)) seq_len {
            uint16_t seq : 13;
            uint16_t len : 11;
        };
        seq_len seq_len{seq_num, len_or_frag_num};
        auto seq_len_begin = reinterpret_cast<uint8_t*>(&seq_len);
        packet.insert(packet.end() - 1, seq_len_begin, seq_len_begin + sizeof(seq_len));
    } else {
        struct __attribute__((packed)) seq_frag_num {
            uint16_t seq : 13;
            uint8_t frag_num : 3;
        };
        seq_frag_num seq_frag_num{seq_num, static_cast<uint8_t>(len_or_frag_num)};
        auto seq_frag_num_begin = reinterpret_cast<uint8_t*>(&seq_frag_num);
        packet.insert(packet.end() - 1, seq_frag_num_begin, seq_frag_num_begin + sizeof(seq_frag_num));
    }
    packet.insert(packet.end(), data.begin(), data.end());
    return packet;
}
