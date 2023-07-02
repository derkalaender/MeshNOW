#include "state.hpp"

#include <esp_event.h>
#include <freertos/FreeRTOS.h>

#include "event_internal.hpp"
#include "util/mac.hpp"

namespace meshnow::state {

static bool root{false};

static util::MacAddr root_mac;
static State state{State::DISCONNECTED_FROM_PARENT};

void setState(State new_state) {
    event::StateChangedData data{
        .old_state = state,
        .new_state = new_state,
    };
    event::fireEvent(event::MESHNOW_INTERNAL, static_cast<int32_t>(event::InternalEvent::STATE_CHANGED), &data,
                     sizeof(state));
}

State getState() { return state; }

void setRoot(bool is_root) { root = is_root; }

bool isRoot() { return root; }

void setRootMac(util::MacAddr mac) { root_mac = mac; }

util::MacAddr getRootMac() {
    // only if we can reach the root the saved mac will be valid
    assert(state == State::REACHES_ROOT);
    return root_mac;
}

util::MacAddr getThisMac() {
    // read once in the first call and cache the result
    static const auto mac = [] {
        util::MacAddr mac;
        esp_read_mac(mac.addr.data(), ESP_MAC_WIFI_STA);
        return mac;
    }();
    return mac;
}

}  // namespace meshnow::state