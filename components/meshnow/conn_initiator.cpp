#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <mutex>

#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("ConnectionInitiator");

static const auto READY_BIT = BIT0;
static const auto AWAIT_VERDICT_BIT = BIT1;

// Frequency at which to send connection beacon (ms)
static const auto CONNECTION_FREQ_MS = 500;

// Min time to wait for potential other parents after the first parent was found (ms)
static const auto FIRST_PARENT_WAIT_MS = 5000;

static const auto MAX_PARENTS_TO_CONSIDER = 5;

meshnow::ConnectionInitiator::ConnectionInitiator(Networking& networking)
    : networking_{networking},
      waitbits_{},
      thread_{&ConnectionInitiator::run, this},
      parent_infos_{},
      first_parent_found_time_{0} {
    parent_infos_.reserve(MAX_PARENTS_TO_CONSIDER);
}

void meshnow::ConnectionInitiator::readyToConnect() {
    std::scoped_lock lock{mtx_};
    ESP_LOGI(TAG, "Ready to connect to a parent");
    waitbits_.setBits(READY_BIT);
}

void meshnow::ConnectionInitiator::stopConnecting() {
    std::scoped_lock lock{mtx_};
    ESP_LOGI(TAG, "Stopping connection attempts");
    parent_infos_.clear();
    waitbits_.clearBits(READY_BIT);
}

void meshnow::ConnectionInitiator::awaitVerdict() {
    std::scoped_lock lock{mtx_};
    ESP_LOGI(TAG, "Awaiting connection verdict");
    waitbits_.setBits(AWAIT_VERDICT_BIT);
}

[[noreturn]] void meshnow::ConnectionInitiator::run() {
    while (true) {
        // wait to be allowed to initiate a connection
        auto bits = waitbits_.waitFor(READY_BIT, false, true, portMAX_DELAY);
        if (!(bits & READY_BIT)) {
            ESP_LOGE(TAG, "Failed to wait for ready bit");
            continue;
        }

        auto last_tick = xTaskGetTickCount();
        {
            //            std::scoped_lock lock{mtx_};
            // ignore if we should stop connecting
            if (!(waitbits_.getBits() & READY_BIT)) continue;

            // send anyone there beacon
            ESP_LOGI(TAG, "Sending anyone there beacon");
            networking_.send_worker_.enqueuePayload(meshnow::BROADCAST_MAC_ADDR,
                                                    std::make_unique<packets::AnyoneTherePayload>());
        }

        // wait for the next cycle
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(CONNECTION_FREQ_MS));

        tryConnect();
    }
}

void meshnow::ConnectionInitiator::tryConnect() {
    {
        std::scoped_lock lock{mtx_};
        // ignore if we should stop connecting
        if (!(waitbits_.getBits() & READY_BIT)) return;

        // check if we found any parents
        if (parent_infos_.empty()) {
            ESP_LOGI(TAG, "No parents found, not trying to connect");
            return;
        }

        // check if we still wait for other potential parents
        auto now = xTaskGetTickCount();
        if (now - first_parent_found_time_ < pdMS_TO_TICKS(FIRST_PARENT_WAIT_MS)) {
            ESP_LOGI(TAG, "Still waiting for other potential parents, not trying to connect");
            return;
        }

        // find the best parent
        auto best_parent = std::max_element(parent_infos_.begin(), parent_infos_.end(),
                                            [](const auto& a, const auto& b) { return a.rssi < b.rssi; });

        // connect to the best parent
        ESP_LOGI(TAG, "Connecting to best parent " MAC_FORMAT " with rssi %d", MAC_FORMAT_ARGS(best_parent->mac_addr),
                 best_parent->rssi);
        // send pls connect payload
        networking_.send_worker_.enqueuePayload(best_parent->mac_addr, std::make_unique<packets::PlsConnectPayload>());
    }
    // wait for verdict
    awaitVerdict();
}

void meshnow::ConnectionInitiator::foundParent(const meshnow::MAC_ADDR& mac_addr, uint8_t rssi) {
    //    std::scoped_lock lock{mtx_};

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

void meshnow::ConnectionInitiator::reject(meshnow::MAC_ADDR mac_addr) {
    std::scoped_lock lock{mtx_};

    // remove parent from list
    auto it = std::find_if(parent_infos_.begin(), parent_infos_.end(),
                           [&mac_addr](const auto& parent_info) { return parent_info.mac_addr == mac_addr; });
    if (it != parent_infos_.end()) {
        ESP_LOGI(TAG, "Removing parent " MAC_FORMAT " from list", MAC_FORMAT_ARGS(mac_addr));
        parent_infos_.erase(it);
    }
}
