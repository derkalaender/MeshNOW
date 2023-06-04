#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "hand_shaker.hpp"
#include "internal.hpp"
#include "packets.hpp"
#include "receive_meta.hpp"
#include "state.hpp"

static const char* TAG = CREATE_TAG("Networking");

// TODO handle list of peers full
static void add_peer(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGV(TAG, "Adding peer " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
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

meshnow::Networking::Networking(std::shared_ptr<NodeState> state)
    : send_worker_(std::make_shared<SendWorker>(layout_)),
      main_worker_(std::make_shared<MainWorker>(send_worker_, layout_, state)) {}

void meshnow::Networking::start() {
    // start both workers
    main_worker_->start();
    send_worker_->start();
}

void meshnow::Networking::stop() {
    ESP_LOGI(TAG, "Stopping main run loop!");

    // TODO this will fail an assert (and crash) because of https://github.com/espressif/esp-idf/issues/10664
    // TODO maybe wrap all of networking in yet another thread which we can safely stop ourselves (no jthread)

    // stop both workers
    main_worker_->stop();
    send_worker_->stop();
}

void meshnow::Networking::rawSend(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data) {
    if (data.size() > MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum data size %d", data.size(), MAX_RAW_PACKET_SIZE);
        throw PayloadTooLargeException();
    }

    // TODO delete unused peers first
    add_peer(mac_addr);
    ESP_LOGV(TAG, "Sending raw data to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), data.data(), data.size()));
}

void meshnow::Networking::onSend(const uint8_t*, esp_now_send_status_t status) {
    ESP_LOGD(TAG, "Send status: %d", status);
    // notify send worker
    send_worker_->sendFinished(status == ESP_NOW_SEND_SUCCESS);
}
