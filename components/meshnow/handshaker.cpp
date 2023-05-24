#include "handshaker.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <mutex>

#include "internal.hpp"
#include "send_worker.hpp"
#include "seq.hpp"

static const char* TAG = CREATE_TAG("Handshaker");

static const auto CONNECT_SUCCESS_BIT = BIT0;
static const auto CONNECT_FAIL_BIT = BIT1;

// Frequency at which to send connection beacon (ms)
static const auto CONNECTION_FREQ_MS = 500;

// Min time to wait for potential other parents after the first parent was found (ms)
static const auto FIRST_PARENT_WAIT_MS = 5000;

static const auto MAX_PARENTS_TO_CONSIDER = 5;

meshnow::Handshaker::Handshaker(SendWorker& send_worker, NodeState& state, routing::Router& router)
    : send_worker_{send_worker}, state_{state}, router_{router}, thread_{&Handshaker::run, this} {
    parent_infos_.reserve(MAX_PARENTS_TO_CONSIDER);
}

[[noreturn]] void meshnow::Handshaker::run() {
    while (true) {
        // wait to be allowed to initiate a connection and acquire state_lock during a whole cycle
        auto state_lock = state_.acquireLock();
        state_.waitForDisconnected(state_lock);

        std::unique_lock sync_lock{sync_.mtx};
        if (sync_.searching) {
            sendBeacon();
            sync_lock.unlock();
            // wait for next cycle
            vTaskDelay(pdMS_TO_TICKS(CONNECTION_FREQ_MS));

            // try connecting if there's suitable parent and if so we want to stop searching and instead wait for the
            // connection reply in the next cycle
            sync_lock.lock();
            sync_.searching = !tryConnect();
        } else {
            sync_lock.unlock();
            // TODO magic constant 50
            // wait for connection reply
            auto bits = sync_.waitbits.waitFor(CONNECT_SUCCESS_BIT | CONNECT_FAIL_BIT, true, false, pdMS_TO_TICKS(50));
            sync_lock.lock();
            if (bits & CONNECT_SUCCESS_BIT) {
                ESP_LOGI(TAG, "Handshake successful");
                // reset the searching flag
                sync_.searching = true;
                // clear found parents
                parent_infos_.clear();
                // we assume we can reach the root because the parent only answers if it itself can reach the root
                state_.setRootReachable(true);
            } else {
                ESP_LOGI(TAG, "Handshake failed, trying to connect to next best parent");
                // simply try to connect to the next best parent
                sync_.searching = !tryConnect();
            }
        }
    }
}

void meshnow::Handshaker::sendBeacon() {
    ESP_LOGI(TAG, "Sending anyone there beacon");
    send_worker_.enqueuePacket(meshnow::BROADCAST_MAC_ADDR, meshnow::packets::Packet{meshnow::generateSequenceNumber(),
                                                                                     meshnow::packets::AnyoneThere{}});
}

bool meshnow::Handshaker::tryConnect() {
    // check if we found any parents
    if (parent_infos_.empty()) {
        ESP_LOGI(TAG, "No parents found, not trying to connect");
        return false;
    }

    // check if we still wait for other potential parents
    auto now = xTaskGetTickCount();
    if (now - first_parent_found_time_ < pdMS_TO_TICKS(FIRST_PARENT_WAIT_MS)) {
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
    send_worker_.enqueuePacket(best_parent->mac_addr, meshnow::packets::Packet{meshnow::generateSequenceNumber(),
                                                                               meshnow::packets::PlsConnect{}});
    // remove it from the list
    parent_infos_.erase(best_parent);
    // TODO check if plsconnect actually arrived
    return true;
}

void meshnow::Handshaker::addPotentialParent(const meshnow::MAC_ADDR& mac_addr, int rssi) {
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

void meshnow::Handshaker::reject(meshnow::MAC_ADDR mac_addr) {
    // remove parent from list
    auto it = std::find_if(parent_infos_.begin(), parent_infos_.end(),
                           [&mac_addr](const auto& parent_info) { return parent_info.mac_addr == mac_addr; });
    if (it != parent_infos_.end()) {
        ESP_LOGI(TAG, "Removing parent " MAC_FORMAT " from list", MAC_FORMAT_ARGS(mac_addr));
        parent_infos_.erase(it);
    }
}

void meshnow::Handshaker::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::AnyoneThere&) {
    // TODO check if cannot accept any more children
    // only offer connection if we have a parent and can reach the root -> disconnected islands won't grow
    if (!state_.isRootReachable()) return;

    ESP_LOGI(TAG, "Sending I am here");
    send_worker_.enqueuePacket(
        meta.src_addr, meshnow::packets::Packet{meshnow::generateSequenceNumber(), meshnow::packets::IAmHere{}});
}

void meshnow::Handshaker::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::IAmHere&) {
    std::scoped_lock sync_lock{sync_.mtx};
    // ignore when already connected (this packet came in late, we already chose a parent)
    if (state_.isConnected()) return;
    addPotentialParent(meta.src_addr, meta.rssi);
}

void meshnow::Handshaker::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::PlsConnect&) {
    auto lock = state_.acquireLock();
    // only accept if we can reach the root
    // this should have been handled by not sending the handleAnyoneThere packet, but race conditions and delays are a
    // thing
    if (!state_.isRootReachable()) return;

    // TODO need some reservation/synchronization mechanism so we don not allocate the same "child slot" to multiple
    // nodes
    ESP_LOGI(TAG, "Sending welcome");
    // TODO check can accept
    auto root_mac = router_.getRootMac();
    assert(root_mac);
    send_worker_.enqueuePacket(meta.src_addr,
                               packets::Packet{meshnow::generateSequenceNumber(), packets::Verdict{*root_mac, true}});
    // TODO wait for packet to be sent

    // TODO add child information

    // send a node connected event to root
    auto res = router_.hopToParent();
    if (res.reached_target_) return;
    assert(res.next_hop_);
    send_worker_.enqueuePacket(
        *res.next_hop_, packets::Packet{meshnow::generateSequenceNumber(), packets::NodeConnected{meta.src_addr}});
}

void meshnow::Handshaker::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::Verdict& p) {
    std::scoped_lock sync_lock{sync_.mtx};
    // ignore if root or already connected (should actually never happen)
    if (state_.isRoot() || state_.isConnected()) return;

    if (p.accept) {
        ESP_LOGI(TAG, "Got accepted by parent: " MAC_FORMAT, MAC_FORMAT_ARGS(meta.src_addr));
        // set the root MAC
        router_.setRootMac(p.root_mac);
        // set parent MAC
        router_.setParentMac(meta.src_addr);

        sync_.waitbits.setBits(CONNECT_SUCCESS_BIT);
    } else {
        ESP_LOGI(TAG, "Got rejected by parent: " MAC_FORMAT, MAC_FORMAT_ARGS(meta.src_addr));
        // remove the possible parent and try connecting to other ones again
        reject(meta.src_addr);

        sync_.waitbits.setBits(CONNECT_FAIL_BIT);
    }
}
