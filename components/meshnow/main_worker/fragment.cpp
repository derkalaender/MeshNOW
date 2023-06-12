#include "fragment.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "constants.hpp"
#include "internal.hpp"

static const char* TAG = CREATE_TAG("Fragment");

static constexpr auto FRAGMENT_TIMEOUT = pdMS_TO_TICKS(3000);

using meshnow::fragment::DataEntry;

DataEntry::DataEntry(uint16_t total_size) : data_(total_size) {
    // calculate number of fragments
    num_fragments = 1;
    if (total_size > MAX_DATA_FIRST_SIZE) {
        num_fragments += 1 + (total_size - MAX_DATA_FIRST_SIZE - 1) / MAX_DATA_NEXT_SIZE;
    }
}

void DataEntry::insertFragment(uint8_t frag_num, const Buffer& data) {
    fragment_mask |= 1 << frag_num;
    if (frag_num == 0) {
        // first fragment
        std::copy(data.begin(), data.end(), data_.begin());
    } else {
        // next fragment
        std::copy(data.begin(), data.end(), data_.begin() + MAX_DATA_FIRST_SIZE + (frag_num - 1) * MAX_DATA_NEXT_SIZE);
    }
    last_fragment_received_ = xTaskGetTickCount();
}

using meshnow::fragment::FragmentTask;

FragmentTask::FragmentTask(std::shared_ptr<lwip::netif::Netif> netif) : netif_(std::move(netif)) {}

TickType_t FragmentTask::nextActionAt() const noexcept {
    // return the time when the next fragment will time out

    auto it = std::min_element(data_entries_.begin(), data_entries_.end(), [](auto&& a, auto&& b) {
        return a.second.lastFragmentReceived() < b.second.lastFragmentReceived();
    });

    if (it == data_entries_.end()) {
        // no entry, don't need to do anything
        return portMAX_DELAY;
    } else {
        return it->second.lastFragmentReceived() + FRAGMENT_TIMEOUT;
    }
}

void FragmentTask::performAction() {
    // remove all entries that have timed out
    auto now = xTaskGetTickCount();
    for (auto it = data_entries_.begin(); it != data_entries_.end();) {
        if (it->second.lastFragmentReceived() + FRAGMENT_TIMEOUT < now) {
            // timed out, remove
            ESP_LOGW(TAG, "Fragment from " MAC_FORMAT " with id %d timed out", MAC_FORMAT_ARGS(it->first.first),
                     it->first.second);
            it = data_entries_.erase(it);
        } else {
            ++it;
        }
    }
}

void FragmentTask::newFragmentFirst(const meshnow::MAC_ADDR& src_mac, uint16_t fragment_id, uint16_t total_size,
                                    const meshnow::Buffer& data) {
    ESP_LOGD(TAG, "Received first fragment. SRC: " MAC_FORMAT " ID: %d SIZE: %d", MAC_FORMAT_ARGS(src_mac), fragment_id,
             total_size);

    // assume the combination of fragment_id and mac is completely unique
    // create a new entry
    DataEntry entry{total_size};
    entry.insertFragment(0, data);
    // check if already complete
    if (entry.isComplete()) {
        dataCompleted(std::move(entry).getData());
    } else {
        // not complete, store it
        data_entries_.emplace(std::make_pair(src_mac, fragment_id), std::move(entry));
    }
}

void FragmentTask::newFragmentNext(const meshnow::MAC_ADDR& src_mac, uint16_t fragment_id, uint8_t frag_num,
                                   const meshnow::Buffer& data) {
    ESP_LOGD(TAG, "Received next fragment. SRC: " MAC_FORMAT " ID: %d NUM: %d", MAC_FORMAT_ARGS(src_mac), fragment_id,
             frag_num);

    // find the entry
    auto it = data_entries_.find(std::make_pair(src_mac, fragment_id));
    if (it == data_entries_.end()) {
        // no entry, ignore
        return;
    }

    // insert the fragment
    it->second.insertFragment(frag_num, data);

    // check if complete
    if (it->second.isComplete()) {
        dataCompleted(std::move(it->second).getData());
        data_entries_.erase(it);
    }
}

void FragmentTask::dataCompleted(meshnow::Buffer&& data) {
    ESP_LOGD(TAG, "Fragment completed. Pushing to IO Driver");
    netif_->io_driver_.driver_impl->receivedData(data);
}