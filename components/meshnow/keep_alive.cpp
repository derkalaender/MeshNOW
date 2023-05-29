#include "keep_alive.hpp"

#include <esp_log.h>

#include "constants.hpp"
#include "internal.hpp"

const char* TAG = CREATE_TAG("KeepAlive");

// send a keep alive beacon every 500ms
static constexpr auto KEEP_ALIVE_BEACON_INTERVAL = pdMS_TO_TICKS(500);

void meshnow::KeepAlive::checkConnections() {
    // TODO
}

void meshnow::KeepAlive::sendKeepAliveBeacon() {
    if (xTaskGetTickCount() - last_beacon_sent_ < KEEP_ALIVE_BEACON_INTERVAL) return;

    // collect all neighbors
    auto neighbors = router_.getChildMacs();
    if (auto parent_mac = router_.getParentMac()) {
        neighbors.push_back(*parent_mac);
    }

    ESP_LOGD(TAG, "Sending keep alive beacons to %d neighbors", neighbors.size());

    // enqueue packet for each neighbor
    for (const auto& mac_addr : neighbors) {
        send_worker_.enqueuePayload(mac_addr, false, packets::KeepAlive{}, SendPromise{}, true, QoS::SINGLE_TRY);
    }

    last_beacon_sent_ = xTaskGetTickCount();
}

TickType_t meshnow::KeepAlive::nextActionIn(TickType_t now) const {
    // TODO

    return portMAX_DELAY;
}

void meshnow::KeepAlive::receivedKeepAliveBeacon(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGD(TAG, "Received keep alive from " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    // TODO
}
