#include "keep_alive.hpp"

#include <esp_log.h>

#include <utility>

#include "state.hpp"

namespace meshnow::job {

static const char* TAG = CREATE_TAG("KeepAlive");

// send a keep alive status every 1s
static constexpr auto STATUS_SEND_INTERVAL = pdMS_TO_TICKS(500);

// consider a neighbor dead if no beacon was received for 3s
static constexpr auto KEEP_ALIVE_TIMEOUT = pdMS_TO_TICKS(2000);

// disconnect from parent if the root was unreachable for 10s
static constexpr auto ROOT_UNREACHABLE_TIMEOUT = pdMS_TO_TICKS(10000);

// StatusSendJob //

TickType_t StatusSendJob::nextActionAt() const noexcept {
    if (getNeighbors(layout_).empty()) return portMAX_DELAY;

    return last_status_sent_ + BEACON_SEND_INTERVAL;
}

void StatusSendJob::performAction() {
    auto now = xTaskGetTickCount();

    if (now - last_status_sent_ < BEACON_SEND_INTERVAL) return;

    std::scoped_lock lock(layout_->mtx);

    auto neighbors = getNeighbors(layout_);
    if (neighbors.empty()) return;
    ESP_LOGD(TAG, "Sending keep alive beacons to %d neighbors", neighbors.size());
    // enqueue packet for each neighbor
    for (auto&& neighbor : neighbors) {
        send_worker_->enqueuePayload(neighbor->mac, false, packets::KeepAlive{}, SendPromise{}, true, QoS::SINGLE_TRY);
    }

    last_status_sent_ = now;
}

// UnreachableTimeoutJob //

TickType_t UnreachableTimeoutJob::nextActionAt() const noexcept {
    return awaiting_reachable ? mesh_unreachable_since_ + ROOT_UNREACHABLE_TIMEOUT : portMAX_DELAY;
}

void UnreachableTimeoutJob::performAction() {
    auto now = xTaskGetTickCount();

    std::scoped_lock lock(layout_->mtx);

    if (awaiting_reachable && now - mesh_unreachable_since_ > ROOT_UNREACHABLE_TIMEOUT) {
        // timeout from waiting for a path to the root

        awaiting_reachable = false;
        assert(layout_->parent);    // parent still has to be there
        layout_->parent = nullptr;  // remove parent
        state_->setConnected(false);
    }
}

void UnreachableTimeoutJob::receivedRootUnreachable() {
    if (state_->isRoot() || !state_->isConnected() || awaiting_reachable) return;

    ESP_LOGI(TAG, "Received root unreachable event, waiting for reachable event");
    state_->setRootReachable(false);
    mesh_unreachable_since_ = xTaskGetTickCount();
    awaiting_reachable = true;

    // stop netif
    netif_->stop();
}

void UnreachableTimeoutJob::receivedRootReachable() {
    if (state_->isRoot() || !state_->isConnected() || !awaiting_reachable) return;

    ESP_LOGI(TAG, "Received root reachable event, sending child connected event");
    // TODO send child connected event
    state_->setRootReachable(true);
    mesh_unreachable_since_ = 0;
    awaiting_reachable = false;

    // start netif again
    netif_->start();
}

// NeighborsAliveCheckTask //

using meshnow::keepalive::NeighborCheckJob;

NeighborCheckJob::NeighborCheckJob(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<NodeState> state,
                                   std::shared_ptr<routing::Layout> layout, std::shared_ptr<lwip::netif::Netif> netif)
    : send_worker_(std::move(send_worker)),
      state_(std::move(state)),
      layout_(std::move(layout)),
      netif_(std::move(netif)) {}

TickType_t NeighborCheckJob::nextActionAt() const noexcept {
    std::scoped_lock lock(layout_->mtx);

    // get the neighbor with the lowest last_seen timestamp

    auto neighbors = getNeighbors(layout_);
    if (neighbors.empty()) return portMAX_DELAY;

    auto n = std::min_element(neighbors.cbegin(), neighbors.cend(),
                              [](auto&& a, auto&& b) { return a->last_seen < b->last_seen; });
    assert(n != neighbors.cend());
    return (*n)->last_seen + KEEP_ALIVE_TIMEOUT;
}

void NeighborCheckJob::performAction() {
    std::scoped_lock lock(layout_->mtx);

    auto neighbors = getNeighbors(layout_);
    if (neighbors.empty()) return;

    auto now = xTaskGetTickCount();

    // go through every neighbor and check if they have timed out
    for (auto&& n : neighbors) {
        if (now - n->last_seen <= KEEP_ALIVE_TIMEOUT) continue;

        auto timed_out_mac = n->mac;

        // neighbor timed out
        if (layout_->parent && layout_->parent->mac == timed_out_mac) {
            // parent timed out
            ESP_LOGW(TAG, "Parent " MAC_FORMAT " timed out", MAC_FORMAT_ARGS(timed_out_mac));
            // parent should still be here
            assert(layout_->parent);
            // disconnect from parent
            layout_->parent = nullptr;
            state_->setConnected(false);
            // send root unreachable to children
            sendRootUnreachable();

            // stop netif
            netif_->stop();
        } else {
            // child timed out
            ESP_LOGW(TAG, "Direct child " MAC_FORMAT " timed out", MAC_FORMAT_ARGS(timed_out_mac));

            if (!routing::removeDirectChild(layout_, timed_out_mac)) {
                ESP_LOGE(TAG, "Failed to remove direct child " MAC_FORMAT, MAC_FORMAT_ARGS(timed_out_mac));
            } else {
                sendChildDisconnected(n->mac);
            }
        }
    }
}

void NeighborCheckJob::sendChildDisconnected(const meshnow::MAC_ADDR& mac_addr) {
    if (state_->isRoot()) return;
    ESP_LOGI(TAG, "Sending child disconnected event");

    // send to root
    send_worker_->enqueuePayload(ROOT_MAC_ADDR, true, packets::NodeDisconnected{mac_addr}, SendPromise{}, true,
                                 QoS::NEXT_HOP);
}

void NeighborCheckJob::sendRootUnreachable() {
    if (layout_->children.empty()) return;
    ESP_LOGI(TAG, "Sending root unreachable event to %d children", layout_->children.size());

    // enqueue packet for each child
    for (auto&& c : layout_->children) {
        send_worker_->enqueuePayload(c->mac, false, packets::RootUnreachable{}, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void NeighborCheckJob::receivedKeepAliveBeacon(const meshnow::MAC_ADDR& from_mac) {
    std::scoped_lock lock(layout_->mtx);
    auto neighbors = getNeighbors(layout_);

    auto n = std::find_if(neighbors.begin(), neighbors.end(),
                          [&from_mac](const auto& neighbor) { return neighbor->mac == from_mac; });

    if (n == neighbors.end()) {
        // ignore any stray beacons
        return;
    }

    // update timestamp
    (*n)->last_seen = xTaskGetTickCount();
}

}  // namespace meshnow::job