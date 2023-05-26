#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "handshaker.hpp"
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

meshnow::Networking::Networking(meshnow::NodeState& state)
    : state_{state}, packet_handler_{*this}, handshaker_{send_worker_, state_, router_} {}

void meshnow::Networking::start() {
    if (state_.isRoot()) {
        // the root can always reach itself
        state_.setRootReachable(true);
    }
    ESP_LOGI(TAG, "Starting main run loop!");
    run_thread_ = std::jthread{[this](std::stop_token stoken) { runLoop(stoken); }};

    // also start the send worker
    send_worker_.start();
}

void meshnow::Networking::stop() {
    ESP_LOGI(TAG, "Stopping main run loop!");

    // TODO this will fail an assert because of https://github.com/espressif/esp-idf/issues/10664
    // TODO maybe wrap all of networking in yet another thread which we can safely stop ourselves (no jthread)

    run_thread_.request_stop();

    // also stop the send worker
    send_worker_.stop();
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

void meshnow::Networking::onSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // TODO
    ESP_LOGD(TAG, "Send status: %d", status);
    // notify send worker
    send_worker_.sendFinished(status == ESP_NOW_SEND_SUCCESS);
}

void meshnow::Networking::onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    ESP_LOGV(TAG, "Received data");

    ReceiveQueueItem item{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), item.from.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), item.to.begin());
    item.rssi = esp_now_info->rx_ctrl->rssi;
    item.data = std::vector<uint8_t>(data, data + data_len);

    // add to receive queue
    receive_queue_.push_back(std::move(item), portMAX_DELAY);
}

TickType_t meshnow::Networking::nextActionIn() const {
    // TODO take min of all timeouts
    auto now = xTaskGetTickCount();
    return handshaker_.nextActionIn(now);
}

void meshnow::Networking::runLoop(const std::stop_token& stoken) {
    while (!stoken.stop_requested()) {
        // calculate timeout from everything that needs to happen after popping from the queue
        auto timeout = nextActionIn();

        ESP_LOGV(TAG, "Next action in at most %lu ticks", timeout);

        // get next packet from receive queue
        auto receive_item = receive_queue_.pop(timeout);
        if (receive_item) {
            // if valid, try to parse
            auto packet = packets::deserialize(receive_item->data);
            if (packet) {
                // if deserialization worked, give packet to packet handler
                ReceiveMeta meta{receive_item->from, receive_item->to, receive_item->rssi, packet->seq_num};
                packet_handler_.handlePacket(meta, packet->payload);
            }
        }

        // TODO check if neighbors are still alive
        // TODO send beacon

        // try to reconnect if not connected
        handshaker_.performHandshake();
    }

    ESP_LOGI(TAG, "Exiting main run loop!");

    // TODO cleanup
}
