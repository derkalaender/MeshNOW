#include "keep_alive.hpp"

#include <esp_log.h>

#include "layout.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/event.hpp"
#include "util/lock.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("KeepAlive");

// send a keep alive status every 1s
static constexpr auto STATUS_SEND_INTERVAL = pdMS_TO_TICKS(500);

// consider a neighbor dead if no beacon was received for 3s
static constexpr auto KEEP_ALIVE_TIMEOUT = pdMS_TO_TICKS(2000);

// disconnect from parent if the root was unreachable for 10s
static constexpr auto ROOT_UNREACHABLE_TIMEOUT = pdMS_TO_TICKS(10000);

// StatusSendJob //

TickType_t StatusSendJob::nextActionAt() const noexcept {
    util::Lock lock{layout::getMtx()};
    if (layout::hasNeighbors()) {
        return last_status_sent_ + STATUS_SEND_INTERVAL;
    } else {
        return portMAX_DELAY;
    }
}

void StatusSendJob::performAction() {
    auto now = xTaskGetTickCount();
    if (now - last_status_sent_ < STATUS_SEND_INTERVAL) return;

    {
        util::Lock lock{layout::getMtx()};
        if (!layout::hasNeighbors()) return;
    }

    sendStatus();

    last_status_sent_ = now;
}

void StatusSendJob::sendStatus() {
    ESP_LOGD(TAG, "Sending status beacons to neighborsSingleTry");
    auto state = state::getState();

    packets::Status payload{
        .state = state,
        .root_mac = state == state::State::REACHES_ROOT ? std::make_optional(state::getRootMac()) : std::nullopt,
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

        awaiting_reachable = false;
        {
            util::Lock lock{layout::getMtx()};
            auto& layout = layout::getLayout();
            assert(layout.parent.has_value());  // parent still has to be there
            layout.parent = std::nullopt;       // remove parent
        }
        state::setState(state::State::DISCONNECTED_FROM_PARENT);  // set state to disconnected
    }
}

void UnreachableTimeoutJob::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != event::MESHNOW_INTERNAL || event_id != event::InternalEvent::STATE_CHANGED) return;

    auto job = static_cast<UnreachableTimeoutJob*>(arg);
    auto new_state = static_cast<event::StateChangedData*>(event_data)->new_state;

    if (job->awaiting_reachable && new_state == state::State::REACHES_ROOT) {
        // root is reachable again
        ESP_LOGI(TAG, "Root is reachable again");
        job->awaiting_reachable = false;
        job->mesh_unreachable_since_ = 0;
    } else if (!job->awaiting_reachable && new_state != state::State::REACHES_ROOT) {
        // root became unreachable
        ESP_LOGI(TAG, "Root became unreachable");
        job->awaiting_reachable = true;
        job->mesh_unreachable_since_ = xTaskGetTickCount();
    }
}

// NeighborsCheckJob //

TickType_t NeighborCheckJob::nextActionAt() const noexcept {
    // get the neighbor with the lowest last_seen timestamp
    auto min_last_seen = portMAX_DELAY;

    {
        util::Lock lock{layout::getMtx()};
        const auto& layout = layout::getLayout();

        for (const auto& child : layout.children) {
            min_last_seen = std::min(min_last_seen, child.last_seen);
        }

        if (layout.parent) min_last_seen = std::min(min_last_seen, layout.parent->last_seen);
    }

    if (min_last_seen == portMAX_DELAY) {
        // no neighbors
        return portMAX_DELAY;
    } else {
        return min_last_seen + KEEP_ALIVE_TIMEOUT;
    }
}

void NeighborCheckJob::performAction() {
    util::Lock lock{layout::getMtx()};

    auto& layout = layout::getLayout();
    auto now = xTaskGetTickCount();

    // check for timeouts in all neighbors and remove from the layout if necessary

    // direct children
    for (auto it = layout.children.begin(); it != layout.children.end();) {
        if (now - it->last_seen > KEEP_ALIVE_TIMEOUT) {
            auto mac = it->mac;
            ESP_LOGW(TAG, "Direct child " MACSTR " timed out", MAC2STR(mac));
            it = layout.children.erase(it);
            // send event upstream
            sendChildDisconnected(mac);
        } else {
            ++it;
        }
    }

    // parent
    if (layout.parent && now - layout.parent->last_seen > KEEP_ALIVE_TIMEOUT) {
        ESP_LOGW(TAG, "Parent " MACSTR " timed out", MAC2STR(layout.parent->mac));
        layout.parent = std::nullopt;
        // update state
        state::setState(state::State::DISCONNECTED_FROM_PARENT);
    }
}

void NeighborCheckJob::sendChildDisconnected(const util::MacAddr& mac) {
    if (state::isRoot()) return;
    {
        util::Lock lock{layout::getMtx()};
        if (!layout::getLayout().parent) return;
    }

    ESP_LOGI(TAG, "Sending child disconnected event upstream");

    // send to parent
    auto payload = packets::NodeDisconnected{.parent_mac = state::getThisMac(), .child_mac = mac};
    send::enqueuePayload(payload, send::SendBehavior::parent(), true);
}

}  // namespace meshnow::job