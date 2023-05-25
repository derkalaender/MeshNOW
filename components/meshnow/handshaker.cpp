#include "handshaker.hpp"

#include <esp_log.h>
#include <freertos/portmacro.h>

#include "internal.hpp"
#include "send_worker.hpp"
#include "seq.hpp"

static const char* TAG = CREATE_TAG("Handshaker");

// Frequency at which to send search probe (ms)
static const auto SEARCH_PROBE_FREQ_MS = 500;

// Time to wait for a connection reply (ms)
static const auto CONNECT_TIMEOUT_MS = 50;

// Min time to wait for potential other parents after the first parent was found (ms)
static const auto FIRST_PARENT_WAIT_MS = 5000;

static const auto MAX_PARENTS_TO_CONSIDER = 5;

meshnow::Handshaker::Handshaker(SendWorker& send_worker, NodeState& state, routing::Router& router)
    : send_worker_{send_worker}, state_{state}, router_{router} {
    parent_infos_.reserve(MAX_PARENTS_TO_CONSIDER);
}

void meshnow::Handshaker::performHandshake() {
    // don't do anything if already connected
    if (state_.isConnected()) return;

    if (searching_for_parents_) {
        // check that we should send a search probe again
        if (xTaskGetTickCount() - last_search_probe_time_ < pdMS_TO_TICKS(SEARCH_PROBE_FREQ_MS)) return;
        sendSearchProbe();
        // immediately try to connect, otherwise searching_for_parents will never be set to false (TODO fix this)
        searching_for_parents_ = !tryConnect();
    } else {
        // check that the last connect request timed out, otherwise return and keep waiting
        if (xTaskGetTickCount() - last_connect_request_time_ < pdMS_TO_TICKS(CONNECT_TIMEOUT_MS)) return;

        // tryConnect() returns true if it sent a connection request so in that case we stop searching for new parents
        searching_for_parents_ = !tryConnect();
    }
}

void meshnow::Handshaker::reset() {
    searching_for_parents_ = true;
    parent_infos_.clear();
    first_parent_found_time_ = 0;
    last_connect_request_time_ = 0;
    last_search_probe_time_ = 0;
}

TickType_t meshnow::Handshaker::nextActionIn(TickType_t now) const {
    // if connected we don't need to do anything
    if (state_.isConnected()) return portMAX_DELAY;

    TickType_t next_action;
    if (searching_for_parents_) {
        // next time until we need to send a search probe
        next_action = last_search_probe_time_ + pdMS_TO_TICKS(SEARCH_PROBE_FREQ_MS);
    } else {
        // next time until we need to send a connect request
        next_action = last_connect_request_time_ + pdMS_TO_TICKS(CONNECT_TIMEOUT_MS);
    }

    // clamp to 0
    return next_action > now ? next_action - now : 0;
}

void meshnow::Handshaker::sendSearchProbe() {
    ESP_LOGI(TAG, "Sending anyone there");
    send_worker_.enqueuePacket(meshnow::BROADCAST_MAC_ADDR, meshnow::packets::Packet{meshnow::generateSequenceNumber(),
                                                                                     meshnow::packets::AnyoneThere{}});
    // update the last time we sent a search probe
    last_search_probe_time_ = xTaskGetTickCount();
}

void meshnow::Handshaker::sendSearchProbeReply() {
    ESP_LOGI(TAG, "Sending i am here");
    send_worker_.enqueuePacket(meshnow::BROADCAST_MAC_ADDR, meshnow::packets::Packet{meshnow::generateSequenceNumber(),
                                                                                     meshnow::packets::IAmHere{}});
}

void meshnow::Handshaker::sendConnectRequest(const MAC_ADDR& mac_addr) {
    ESP_LOGI(TAG, "Sending connect request to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    send_worker_.enqueuePacket(
        mac_addr, meshnow::packets::Packet{meshnow::generateSequenceNumber(), meshnow::packets::PlsConnect{}});
}

void meshnow::Handshaker::sendConnectReply(const MAC_ADDR& mac_addr, bool accept) {
    ESP_LOGI(TAG, "Sending verdict to " MAC_FORMAT ": %s", MAC_FORMAT_ARGS(mac_addr),
             accept ? "accept" : "rejectParent");
    auto root_mac = router_.getRootMac();
    // we can be sure that the root mac is set because we only send a verdict if we have a parent
    assert(root_mac);
    send_worker_.enqueuePacket(mac_addr, meshnow::packets::Packet{meshnow::generateSequenceNumber(),
                                                                  meshnow::packets::Verdict{root_mac.value(), accept}});
}

bool meshnow::Handshaker::tryConnect() {
    // check if we found any parents
    if (parent_infos_.empty()) {
        ESP_LOGI(TAG, "No parents found, not trying to connect");
        return false;
    }

    // check if we still wait for other potential parents
    if (xTaskGetTickCount() - first_parent_found_time_ < pdMS_TO_TICKS(FIRST_PARENT_WAIT_MS)) {
        ESP_LOGI(TAG, "Still waiting for other potential parents, not trying to connect");
        return false;
    }

    // find the best parent
    auto best_parent = std::max_element(parent_infos_.begin(), parent_infos_.end(),
                                        [](const auto& a, const auto& b) { return a.rssi < b.rssi; });

    // connect to the best parent
    ESP_LOGI(TAG, "Connecting to best parent " MAC_FORMAT " with rssi %d", MAC_FORMAT_ARGS(best_parent->mac_addr),
             best_parent->rssi);

    // send pls connect payload
    sendConnectRequest(best_parent->mac_addr);

    // set the current time as the last time we sent a connection request
    // this allows us to timeout if we don't get a reply
    last_connect_request_time_ = xTaskGetTickCount();

    // remove the best parent from the list because we don't want to reconnect in case of failure
    parent_infos_.erase(best_parent);
    // TODO check if plsconnect actually arrived
    return true;
}

void meshnow::Handshaker::foundPotentialParent(const MAC_ADDR& mac_addr, int rssi) {
    // ignore when already connected or not searching anymore (this packet came in late, we already chose a parent)
    if (state_.isConnected() || !searching_for_parents_) return;

    if (parent_infos_.empty()) {
        first_parent_found_time_ = xTaskGetTickCount();
    }

    // check if we already know this parent
    auto it = std::find_if(parent_infos_.begin(), parent_infos_.end(),
                           [&mac_addr](const auto& parent_info) { return parent_info.mac_addr == mac_addr; });
    if (it != parent_infos_.end()) {
        ESP_LOGI(TAG, "Updating parent " MAC_FORMAT " with rssi %d->%d", MAC_FORMAT_ARGS(mac_addr), it->rssi, rssi);

        // update rssi
        it->rssi = rssi;
        return;
    } else {
        ESP_LOGI(TAG, "Found new parent " MAC_FORMAT " with rssi %d", MAC_FORMAT_ARGS(mac_addr), rssi);

        if (parent_infos_.size() == MAX_PARENTS_TO_CONSIDER) {
            // replace worst parent
            auto worst_parent = std::min_element(parent_infos_.begin(), parent_infos_.end(),
                                                 [](const auto& a, const auto& b) { return a.rssi < b.rssi; });
            worst_parent->mac_addr = mac_addr;
            worst_parent->rssi = rssi;
        } else {
            // add new parent
            parent_infos_.push_back({mac_addr, rssi});
        }
    }
}

void meshnow::Handshaker::rejectParent(MAC_ADDR mac_addr) {
    // remove parent from list
    auto it = std::find_if(parent_infos_.begin(), parent_infos_.end(),
                           [&mac_addr](const auto& parent_info) { return parent_info.mac_addr == mac_addr; });
    if (it != parent_infos_.end()) {
        ESP_LOGI(TAG, "Removing parent " MAC_FORMAT " from list", MAC_FORMAT_ARGS(mac_addr));
        parent_infos_.erase(it);
    }
}

void meshnow::Handshaker::receivedConnectResponse(const MAC_ADDR& mac_addr, bool accept,
                                                  std::optional<MAC_ADDR> root_mac_addr) {
    // ignore if root or already connected (should actually never happen)
    if (state_.isRoot() || state_.isConnected()) return;

    if (accept) {
        ESP_LOGI(TAG, "Got accepted by parent: " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
        // if accept we have the root mac
        assert(root_mac_addr);

        // set the root MAC
        router_.setRootMac(root_mac_addr.value());
        // set parent MAC
        router_.setParentMac(mac_addr);

        // update the state to connected
        state_.setConnected(true);
        // newly connected, we can reach the root
        state_.setRootReachable(true);

        // reset everything for the next handshake when we might disconnect
        reset();
    } else {
        ESP_LOGI(TAG, "Got rejected by parent: " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
        // remove this parent because it failed us :(
        rejectParent(mac_addr);

        // set the tick time to 0 so that we try to connect to another parent
        last_connect_request_time_ = 0;
    }
}

void meshnow::Handshaker::receivedSearchProbe(const MAC_ADDR& mac_addr) {
    // TODO check if cannot accept any more children
    // only offer connection if we have a parent and can reach the root -> disconnected islands won't grow
    if (!state_.isRootReachable()) return;

    sendSearchProbeReply();
}

void meshnow::Handshaker::receivedConnectRequest(const MAC_ADDR& mac_addr) {
    // only accept if we can reach the root
    // this is necessary because we may have disconnected since we answered the search probe
    if (!state_.isRootReachable()) return;

    // TODO need some reservation/synchronization mechanism so we don not allocate the same "child slot" to multiple
    // nodes
    // TODO check can accept
    sendConnectReply(mac_addr, true);
    // TODO wait for packet to be sent

    // TODO add child information

    // send a node connected event to root
    // TODO refactor into own method
    auto res = router_.hopToParent();
    if (res.reached_target_) return;
    assert(res.next_hop_);
    send_worker_.enqueuePacket(*res.next_hop_,
                               packets::Packet{meshnow::generateSequenceNumber(), packets::NodeConnected{mac_addr}});
}
