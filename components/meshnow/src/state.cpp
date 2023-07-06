#include "state.hpp"

#include "event_internal.hpp"
#include "layout.hpp"
#include "packets.hpp"
#include "send/queue.hpp"
#include "util/lock.hpp"
#include "util/mac.hpp"

namespace meshnow::state {

static bool root{false};

static util::MacAddr root_mac;
static State state{State::DISCONNECTED_FROM_PARENT};

void setState(State new_state) {
    if (new_state == state) return;

    event::StateChangedData data{
        .old_state = state,
        .new_state = new_state,
    };

    state = new_state;

    event::fireEvent(event::MESHNOW_INTERNAL, static_cast<int32_t>(event::InternalEvent::STATE_CHANGED), &data,
                     sizeof(state));

    // don't send root reachable status events downstream if no children
    if (!layout::Layout::get().hasChildren()) return;

    // send to all children downstream
    if (new_state == State::REACHES_ROOT) {
        auto payload = packets::RootReachable{.root = getRootMac()};
        send::enqueuePayload(payload, send::SendBehavior::children(), true);
    } else {
        assert(!isRoot());
        auto payload = packets::RootUnreachable{};
        send::enqueuePayload(payload, send::SendBehavior::children(), true);
    }
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