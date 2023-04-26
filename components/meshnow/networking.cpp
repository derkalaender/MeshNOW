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
    if (payload.size() > MAX_RAW_PAYLOAD_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum payload size %d", payload.size(), MAX_RAW_PAYLOAD_SIZE);
        throw PayloadTooLargeException();
    }

    add_peer(mac_addr);
    ESP_LOGI(TAG, "Sending raw payload to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), payload.data(), payload.size()));
}
