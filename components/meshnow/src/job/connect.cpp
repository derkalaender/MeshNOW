#include "connect.hpp"

#include <esp_log.h>
#include <esp_wifi.h>

#include "layout.hpp"
#include "packets.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/lock.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("Connect");

// How long to wait before sending a new search probe
static constexpr auto SEARCH_PROBE_INTERVAL = pdMS_TO_TICKS(50);

// How many probes to send per channel before switching
static constexpr auto PROBES_PER_CHANNEL = 3;

// Min time to wait for potential other parents after the first parent was found
static constexpr auto FIRST_PARENT_WAIT = pdMS_TO_TICKS(3000);

static constexpr auto MAX_PARENTS_TO_CONSIDER = 5;

// Time to wait for a connection reply
static constexpr auto CONNECT_TIMEOUT = pdMS_TO_TICKS(50);

ConnectJob::ConnectJob()
    :  // use lambda to initialize channel config
      channel_config_([] {
          // get channel config via wifi country
          wifi_country_t country;
          ESP_ERROR_CHECK(esp_wifi_get_country(&country));
          uint8_t min_channel = country.schan;
          uint8_t max_channel = min_channel + country.nchan - 1;
          return ChannelConfig{min_channel, max_channel};
      }()) {
    // root never performs any connecting process
    if (state::isRoot()) return;

    // we start with the search phase
    phase_ = std::make_unique<SearchPhase>(*this);
}

TickType_t ConnectJob::nextActionAt() const noexcept {
    // root never performs any connecting process
    if (state::isRoot() || state::getState() == state::State::REACHES_ROOT) return portMAX_DELAY;
    return phase_->nextActionAt();
}

void ConnectJob::performAction() {
    // root never performs any connecting process
    if (state::isRoot() || state::getState() == state::State::REACHES_ROOT) return;
    return phase_->performAction();
}

// SEARCH PHASE //

TickType_t ConnectJob::SearchPhase::nextActionAt() const noexcept {
    if (last_search_probe_time_ == 0) return 0;
    // we always want to send a search probe
    return last_search_probe_time_ + SEARCH_PROBE_INTERVAL;
}

void ConnectJob::SearchPhase::performAction() {
    if (xTaskGetTickCount() - last_search_probe_time_ < SEARCH_PROBE_INTERVAL) return;

    if (job_.parent_infos_.empty()) {
        // if we haven't found a parent yet, we switch the channel after a set amount of probes
        if (search_probes_sent_ >= PROBES_PER_CHANNEL) {
            // switch to next channel
            current_channel_++;
            // potentially wrap around
            if (current_channel_ > job_.channel_config_.max_channel) {
                current_channel_ = job_.channel_config_.min_channel;
            }
            ESP_LOGI(TAG, "Switching to channel %d", current_channel_);
            ESP_ERROR_CHECK(esp_wifi_set_channel(current_channel_, WIFI_SECOND_CHAN_NONE));
            search_probes_sent_ = 0;
        }
    } else {
        // have found at least one parent
        // keep searching for new ones, but after a while start the connecting phase
        if (xTaskGetTickCount() - first_parent_found_time_ > FIRST_PARENT_WAIT) {
            ESP_LOGI(TAG, "Starting connecting phase");
            job_.phase_ = std::make_unique<ConnectPhase>(job_);
        }
    }

    // send a probe
    last_search_probe_time_ = xTaskGetTickCount();
    search_probes_sent_++;
    sendSearchProbe();
}

void ConnectJob::SearchPhase::event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                            void *event_data) {
    if (event_base != event::MESHNOW_INTERNAL || event_id != event::InternalEvent::PARENT_FOUND) return;

    auto search = static_cast<SearchPhase *>(event_handler_arg);
    auto parent_data = static_cast<event::ParentFoundData *>(event_data);

    auto parent_mac = *parent_data->mac;
    auto parent_rssi = parent_data->rssi;

    // check if the advertised parent is already in the Layout, if so, ignore
    {
        util::Lock lock{layout::getMtx()};
        if (layout::has(parent_mac)) return;
    }

    auto &parent_infos = search->job_.parent_infos_;

    // check if we already know this parent
    auto it = std::find_if(parent_infos.begin(), parent_infos.end(),
                           [&parent_mac](const ParentInfo &info) { return info.mac_addr == parent_mac; });

    if (it != parent_infos.end()) {
        ESP_LOGI(TAG, "Updating parent " MACSTR ". RSSI %d(old) -> %d(new)", MAC2STR(parent_mac), it->rssi,
                 parent_rssi);

        // update the RSSI
        it->rssi = parent_rssi;
    } else {
        ESP_LOGI(TAG, "Found new parent " MACSTR ". RSSI %d", MAC2STR(parent_mac), parent_rssi);

        // add the parent to the list, replacing the weakest parent if the list is full
        if (parent_infos.size() >= MAX_PARENTS_TO_CONSIDER) {
            // find the weakest parent
            auto weakest_it =
                std::min_element(parent_infos.begin(), parent_infos.end(),
                                 [](const ParentInfo &a, const ParentInfo &b) { return a.rssi < b.rssi; });
            // replace it with the new parent
            ESP_LOGI(TAG, "Replacing parent " MACSTR " with " MACSTR, MAC2STR(weakest_it->mac_addr),
                     MAC2STR(parent_mac));
            *weakest_it = ParentInfo{parent_mac, parent_rssi};
        } else {
            // just add the new parent
            parent_infos.push_back(ParentInfo{parent_mac, parent_rssi});
        }
    }

    // if we have found the first parent, remember the time
    if (parent_infos.size() == 1) {
        search->first_parent_found_time_ = xTaskGetTickCount();
    }
}

void ConnectJob::SearchPhase::sendSearchProbe() {
    ESP_LOGI(TAG, "Broadcasting search probe");
    send::enqueuePayload(packets::AnyoneThere{}, send::SendBehavior::direct(util::MacAddr::broadcast()), true);
}

// CONNECT PHASE //

TickType_t ConnectJob::ConnectPhase::nextActionAt() const noexcept {
    if (awaiting_connect_response_) {
        return last_connect_request_time_ + CONNECT_TIMEOUT;
    } else {
        return 0;
    }
}

void ConnectJob::ConnectPhase::performAction() {
    if (awaiting_connect_response_) {
        // check if we have timed out
        if (xTaskGetTickCount() - last_connect_request_time_ > CONNECT_TIMEOUT) {
            ESP_LOGI(TAG, "Connect request timed out");
            awaiting_connect_response_ = false;
        }
    }

    // send a connect request to the best potential parent

    // get best parent
    auto it = std::max_element(job_.parent_infos_.begin(), job_.parent_infos_.end(),
                               [](const ParentInfo &a, const ParentInfo &b) { return a.rssi < b.rssi; });
    if (it == job_.parent_infos_.end()) {
        ESP_LOGI(TAG, "All parents exhausted, returning to search phase");
        job_.phase_ = std::make_unique<SearchPhase>(job_);
        return;
    }

    current_parent_mac_ = it->mac_addr;
    job_.parent_infos_.erase(it);

    awaiting_connect_response_ = true;
    last_connect_request_time_ = xTaskGetTickCount();
    sendConnectRequest(current_parent_mac_);
}

void ConnectJob::ConnectPhase::event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                             void *event_data) {
    if (event_base != event::MESHNOW_INTERNAL || event_id != event::InternalEvent::GOT_CONNECT_RESPONSE) return;

    auto connect = static_cast<ConnectPhase *>(event_handler_arg);
    auto response_data = static_cast<event::GotConnectResponseData *>(event_data);

    // got a wrong connection response
    if (*response_data->mac != connect->current_parent_mac_) return;

    // got a correct connection response
    ESP_LOGI(TAG, "Got connect response from " MACSTR "%s.", MAC2STR(*response_data->mac),
             response_data->accepted ? "Accepted" : "Rejected");
    connect->awaiting_connect_response_ = false;

    if (response_data->accepted) {
        // we are now connected to the parent
        // set parent info
        {
            util::Lock lock{layout::getMtx()};
            auto &layout = layout::getLayout();
            layout.parent = std::make_optional<layout::Neighbor>(*response_data->mac);
        }
        // set root mac
        state::setRootMac(*response_data->mac);

        // assume we can reach the root already because otherwise the parent would not have accepted us
        state::setState(state::State::REACHES_ROOT);

        // reset to search phase in case we disconnect again
        connect->job_.phase_ = std::make_unique<SearchPhase>(connect->job_);
    }

    // if we weren't accepted, then the next parent will be tried in the next action
}

void ConnectJob::ConnectPhase::sendConnectRequest(const util::MacAddr &to_mac) {
    ESP_LOGI(TAG, "Sending connect request to " MACSTR, MAC2STR(to_mac));
    send::enqueuePayload(packets::ConnectRequest{}, send::SendBehavior::direct(to_mac), true);
}

}  // namespace meshnow::job
