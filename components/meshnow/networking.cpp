#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "internal.hpp"
#include "packets.hpp"

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

void meshnow::Networking::raw_broadcast(const std::vector<uint8_t>& payload) { raw_send(BROADCAST_MAC_ADDR, payload); }
void meshnow::Networking::raw_send(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& payload) {
    if (payload.size() > MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum payload size %d", payload.size(), MAX_RAW_PACKET_SIZE);
        throw PayloadTooLargeException();
    }

    add_peer(mac_addr);
    ESP_LOGI(TAG, "Sending raw payload to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), payload.data(), payload.size()));
}

void meshnow::Networking::on_send(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // TODO
    ESP_LOGI(TAG, "Send status: %d", status);
}

void meshnow::Networking::on_receive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    ESP_LOGI(TAG, "Received data");
    // TODO error checking and timeout

    ReceiveMeta meta{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), meta.src_addr.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), meta.dest_addr.begin());
    meta.rssi = esp_now_info->rx_ctrl->rssi;

    auto buffer = std::vector<uint8_t>(data, data + data_len);
    auto payload = packets::Packet::Packet::deserialize(buffer);
    if (!payload) {
        // received invalid payload
        return;
    }

    // this will in turn call one of the handle functions
    payload->handle(*this, meta);
}

// TODO
[[noreturn]] void meshnow::Networking::ReceiveWorker() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void meshnow::Networking::handleStillAlive(const ReceiveMeta& meta) {}

void meshnow::Networking::handleAnyoneThere(const ReceiveMeta& meta) {}

void meshnow::Networking::handleIAmHere(const ReceiveMeta& meta) {}

void meshnow::Networking::handlePlsConnect(const ReceiveMeta& meta) {}

void meshnow::Networking::handleWelcome(const ReceiveMeta& meta) {}

void meshnow::Networking::handleNodeConnected(const ReceiveMeta& meta, const packets::NodeConnectedPayload& payload) {}

void meshnow::Networking::handleNodeDisconnected(const ReceiveMeta& meta,
                                                 const packets::NodeDisconnectedPayload& payload) {}

void meshnow::Networking::handleMeshUnreachable(const ReceiveMeta& meta) {}

void meshnow::Networking::handleMeshReachable(const ReceiveMeta& meta) {}

void meshnow::Networking::handleDataAck(const ReceiveMeta& meta, const packets::DataAckPayload& payload) {}

void meshnow::Networking::handleDataNack(const ReceiveMeta& meta, const packets::DataNackPayload& payload) {}

void meshnow::Networking::handleDataFirst(const ReceiveMeta& meta, const packets::DataFirstPayload& payload) {}

void meshnow::Networking::handleDataNext(const ReceiveMeta& meta, const packets::DataNextPayload& payload) {}
