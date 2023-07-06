#include "keep_alive.hpp"

#include <esp_log.h>

#include "layout.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("KeepAlive");

// send a keep alive status every 1s
static constexpr auto STATUS_SEND_INTERVAL = pdMS_TO_TICKS(500);

// consider a neighbor dead if no beacon was received for 3s
static constexpr auto KEEP_ALIVE_TIMEOUT = pdMS_TO_TICKS(3000);

// disconnect from parent if the root was unreachable for 10s
static constexpr auto ROOT_UNREACHABLE_TIMEOUT = pdMS_TO_TICKS(10000);

// StatusSendJob //

TickType_t StatusSendJob::nextActionAt() const noexcept {
    if (!layout::Layout::get().isEmpty()) {
        return last_status_sent_ + STATUS_SEND_INTERVAL;
    } else {
        return portMAX_DELAY;
    }
}

void StatusSendJob::performAction() {
    auto now = xTaskGetTickCount();
    if (now - last_status_sent_ < STATUS_SEND_INTERVAL) return;

    if (layout::Layout::get().isEmpty()) return;

    sendStatus();

    last_status_sent_ = now;
}

void StatusSendJob::sendStatus() {
    ESP_LOGD(TAG, "Sending status beacons to neighbors");
    auto state = state::getState();

    packets::Status payload{
        .state = state,
        .root = state == state::State::REACHES_ROOT ? std::make_optional(state::getRootMac()) : std::nullopt,
    };

    send::enqueuePayload(payload, send::SendBehavior::neighborsSingleTry(), true);
}

// UnreachableTimeoutJob //

TickType_t UnreachableTimeoutJob::nextActionAt() const noexcept {
    return awaiting_reachable ? mesh_unreachable_since_ + ROOT_UNREACHABLE_TIMEOUT : portMAX_DELAY;
}

void UnreachableTimeoutJob::performAction() {
    auto now = xTaskGetTickCount();

    if (awaiting_reachable && now - mesh_unreachable_since_ > ROOT_UNREACHABLE_TIMEOUT) {
        // timeout from waiting for a path to the root

        ESP_LOGI(TAG, "Timeout from waiting for a path to the root");

        awaiting_reachable = false;

        // if we haven't lost the parent by now because of a Keep Alive timeout, remove it
        if (auto& parent = layout::Layout::get().getParent()) {
            parent = std::nullopt;
            state::setState(state::State::DISCONNECTED_FROM_PARENT);  // set state to disconnected
        }
    }
}

void UnreachableTimeoutJob::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != event::MESHNOW_INTERNAL || event_id != event::InternalEvent::STATE_CHANGED) return;

    auto& job = *static_cast<UnreachableTimeoutJob*>(arg);
    auto& data = *static_cast<event::StateChangedData*>(event_data);
    auto new_state = data.new_state;
    auto old_state = data.old_state;

    if (job.awaiting_reachable) {
        if (old_state == state::State::CONNECTED_TO_PARENT && new_state == state::State::REACHES_ROOT) {
            // root is reachable again
            ESP_LOGI(TAG, "Root is reachable again");
        }
        job.awaiting_reachable = false;
        job.mesh_unreachable_since_ = 0;
    } else {
        if (old_state == state::State::REACHES_ROOT && new_state == state::State::CONNECTED_TO_PARENT) {
            // root became unreachable
            ESP_LOGI(TAG, "Root became unreachable");
            job.awaiting_reachable = true;
            job.mesh_unreachable_since_ = xTaskGetTickCount();
        }
    }
}

// NeighborsCheckJob //

TickType_t NeighborCheckJob::nextActionAt() const noexcept {
    // get the neighbor with the lowest last_seen timestamp
    auto min_last_seen = portMAX_DELAY;

    auto& layout = layout::Layout::get();

    for (const auto& child : layout.getChildren()) {
        min_last_seen = std::min(min_last_seen, child.last_seen);
    }

    if (auto& parent = layout.getParent()) min_last_seen = std::min(min_last_seen, parent->last_seen);

    if (min_last_seen == portMAX_DELAY) {
        // no neighbors
        return portMAX_DELAY;
    } else {
        return min_last_seen + KEEP_ALIVE_TIMEOUT;
    }
}

void NeighborCheckJob::performAction() {
    auto& layout = layout::Layout::get();
    auto now = xTaskGetTickCount();

    // check for timeouts in all neighbors and remove from the layout if necessary

    // direct children
    for (auto it = layout.getChildren().begin(); it != layout.getChildren().end();) {
        if (now - it->last_seen > KEEP_ALIVE_TIMEOUT) {
            auto mac = it->mac;
            ESP_LOGW(TAG, "Direct child " MACSTR " timed out", MAC2STR(mac));
            layout.removeChild(it->mac);
            // send event upstream
            sendChildDisconnected(mac);
        } else {
            ++it;
        }
    }

    // parent
    if (auto& parent = layout.getParent(); parent && now - parent->last_seen > KEEP_ALIVE_TIMEOUT) {
        ESP_LOGW(TAG, "Parent " MACSTR " timed out", MAC2STR(parent->mac));
        parent = std::nullopt;
        // update state
        state::setState(state::State::DISCONNECTED_FROM_PARENT);
    }
}

void NeighborCheckJob::sendChildDisconnected(const util::MacAddr& mac) {
    if (state::isRoot()) return;

    if (!layout::Layout::get().getParent()) return;

    ESP_LOGI(TAG, "Sending child disconnected event upstream");

    // send to parent
    auto payload = packets::RemoveFromRoutingTable{mac};
    send::enqueuePayload(payload, send::SendBehavior::parent(), true);
}

}  // namespace meshnow::job