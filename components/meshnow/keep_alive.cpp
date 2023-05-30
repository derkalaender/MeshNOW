#include "keep_alive.hpp"

#include <esp_log.h>

#include "constants.hpp"
#include "internal.hpp"

const char* TAG = CREATE_TAG("KeepAlive");

// send a keep alive beacon every 500ms
static constexpr auto KEEP_ALIVE_BEACON_INTERVAL = pdMS_TO_TICKS(300);

// consider a neighbor dead if no beacon was received for 2s
static constexpr auto KEEP_ALIVE_TIMEOUT = pdMS_TO_TICKS(3000);

// disconnect from parent if the root was unreachable for 10s
static constexpr auto ROOT_UNREACHABLE_TIMEOUT = pdMS_TO_TICKS(10000);

void meshnow::KeepAlive::checkConnections() {
    auto now = xTaskGetTickCount();

    if (awaiting_reachable && now - mesh_unreachable_since_ > ROOT_UNREACHABLE_TIMEOUT) {
        awaiting_reachable = false;
        auto parent_mac = router_.getParentMac();
        assert(parent_mac);
        stopTrackingNeighbor(*parent_mac);
        state_.setConnected(false);
    }

    // iterate while erasing
    for (auto it = neighbors.cbegin(); it != neighbors.cend();) {
        auto [mac_addr, last_beacon_received] = *it;
        if (now - last_beacon_received > KEEP_ALIVE_TIMEOUT) {
            it = neighbors.erase(it);
            ESP_LOGW(TAG, "Neighbor " MAC_FORMAT " timed out", MAC_FORMAT_ARGS(mac_addr));
            auto result = router_.removeNeighbor(mac_addr);
            if (result == routing::Router::RemoveResult::PARENT) {
                sendMeshUnreachable();
                state_.setConnected(false);
            } else if (result == routing::Router::RemoveResult::CHILD) {
                sendChildDisconnected(mac_addr);
            }
        } else {
            ++it;
        }
    }
}

void meshnow::KeepAlive::sendKeepAliveBeacon() {
    if (xTaskGetTickCount() - last_beacon_sent_ < KEEP_ALIVE_BEACON_INTERVAL) return;
    if (neighbors.empty()) return;

    ESP_LOGD(TAG, "Sending keep alive beacons to %d neighbors", neighbors.size());

    // enqueue packet for each neighbor
    for (auto& [mac_addr, _] : neighbors) {
        send_worker_.enqueuePayload(mac_addr, false, packets::KeepAlive{}, SendPromise{}, true, QoS::SINGLE_TRY);
    }

    last_beacon_sent_ = xTaskGetTickCount();
}

TickType_t meshnow::KeepAlive::nextActionIn(TickType_t now) const {
    // don't do anything if there are no neighbors
    if (neighbors.empty()) return portMAX_DELAY;

    auto next_beacon = last_beacon_sent_ + KEEP_ALIVE_BEACON_INTERVAL;
    // get the next timeout from neighbors, or portMAX_DELAY if there are none
    auto it = std::min_element(neighbors.begin(), neighbors.end(),
                               [](const auto& a, const auto& b) { return a.second < b.second; });
    auto next_timeout = it != neighbors.end() ? it->second + KEEP_ALIVE_TIMEOUT : portMAX_DELAY;

    auto next_root_unreachable_timeout =
        awaiting_reachable ? mesh_unreachable_since_ + ROOT_UNREACHABLE_TIMEOUT : portMAX_DELAY;

    auto next_action = std::min({next_beacon, next_timeout, next_root_unreachable_timeout});
    // clamp to 0
    return next_action > now ? next_action - now : 0;
}

void meshnow::KeepAlive::receivedKeepAliveBeacon(const meshnow::MAC_ADDR& mac_addr) {
    // ignore any stray beacons
    if (!neighbors.contains(mac_addr)) return;

    ESP_LOGD(TAG, "Received keep alive from neighbor " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));

    neighbors[mac_addr] = xTaskGetTickCount();
}

void meshnow::KeepAlive::trackNeighbor(const MAC_ADDR& mac_addr) {
    ESP_LOGI(TAG, "Starting tracking neighbor " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    neighbors[mac_addr] = xTaskGetTickCount();
}

void meshnow::KeepAlive::stopTrackingNeighbor(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGI(TAG, "Stopping tracking neighbor " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    neighbors.erase(mac_addr);
}

void meshnow::KeepAlive::sendMeshUnreachable() {
    if (neighbors.empty()) return;
    ESP_LOGI(TAG, "Sending mesh unreachable event to %d neighbors", neighbors.size());

    // enqueue packet for each neighbor
    for (auto& [mac_addr, _] : neighbors) {
        send_worker_.enqueuePayload(mac_addr, false, packets::MeshUnreachable{}, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void meshnow::KeepAlive::sendChildDisconnected(const meshnow::MAC_ADDR& mac_addr) {
    if(state_.isRoot()) return;
    ESP_LOGI(TAG, "Sending child disconnected event");

    // send to root
    send_worker_.enqueuePayload(ROOT_MAC_ADDR, true, packets::NodeDisconnected{mac_addr}, SendPromise{}, true,
                                QoS::NEXT_HOP);
}

void meshnow::KeepAlive::receivedMeshUnreachable() {
    if (state_.isRoot() || !state_.isConnected() || awaiting_reachable) return;
    ESP_LOGI(TAG, "Received mesh unreachable event, waiting for reachable event");
    state_.setRootReachable(false);
    mesh_unreachable_since_ = xTaskGetTickCount();
    awaiting_reachable = true;
}

void meshnow::KeepAlive::receivedMeshReachable() {
    if (state_.isRoot() || !state_.isConnected() || !awaiting_reachable) return;
    ESP_LOGI(TAG, "Received mesh reachable event, sending child connected event");
    state_.setRootReachable(true);
    mesh_unreachable_since_ = 0;
    awaiting_reachable = false;
}
