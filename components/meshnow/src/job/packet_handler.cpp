#include "packet_handler.hpp"

#include <esp_log.h>

#include "event_internal.hpp"
#include "fragments.hpp"
#include "layout.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("PacketHandler");

void PacketHandler::handlePacket(const util::MacAddr& from, int rssi, const packets::Packet& packet) {
    // TODO handle duplicate packets
    // TODO update routing table

    auto payload = packet.payload;

    // simply visit the corresponding overload
    std::visit(
        [&](const auto& p) {
            // little hack to check at compiletime if specific handle function also accepts the rssi value
            if constexpr (requires {
                              { handle(util::MacAddr::root(), rssi, p) };
                          }) {
                handle(from, rssi, p);
            } else {
                handle(from, p);
            }
        },
        payload);
}

/**
 * Helper functions
 */
namespace {

inline layout::Layout& layout() { return layout::Layout::get(); }

inline bool reachesRoot() {
    if (state::getState() == state::State::REACHES_ROOT) {
        assert((state::isRoot() || layout().getParent()) && "By this point, must have either a parent or be the root");
        return true;
    } else {
        return false;
    }
}

inline bool knowsNode(const util::MacAddr& mac) { return layout().has(mac); }

inline bool isParent(const util::MacAddr& mac) {
    auto& parent = layout().getParent();
    if (parent) {
        assert(state::getState() != state::State::DISCONNECTED_FROM_PARENT &&
               "Must not be disconnected from parent if there is a parent");
        return parent->mac == mac;
    }
    return false;
}

inline bool isChild(const util::MacAddr& mac) {
    auto children = layout().getChildren();
    return std::any_of(children.begin(), children.end(), [&](const layout::Child& child) { return child.mac == mac; });
}

inline bool isNeighbor(const util::MacAddr& mac) { return isParent(mac) || isChild(mac); }

inline bool canAcceptNewChild() { return layout().getChildren().size() < layout::MAX_CHILDREN; }

inline bool disconnected() {
    if (state::getState() == state::State::DISCONNECTED_FROM_PARENT) {
        assert(!state::isRoot() && "Cannot be disconnected and root at the same time");
        assert(!layout().getParent() && "Cannot have a parent by this point");
        return true;
    } else {
        return false;
    }
}

}  // namespace

// HANDLERS //

void PacketHandler::handle(const util::MacAddr& from, const packets::Status& p) {
    auto& layout = layout::Layout::get();

    // is parent?
    if (auto& parent = layout.getParent(); parent && parent->mac == from) {
        // got status from parent
        parent->last_seen = xTaskGetTickCount();
        switch (p.state) {
            case state::State::DISCONNECTED_FROM_PARENT:
            case state::State::CONNECTED_TO_PARENT: {
                state::setState(state::State::CONNECTED_TO_PARENT);
                break;
            }
            case state::State::REACHES_ROOT: {
                // this should never be the case, but we never know with malicious packets
                if (!p.root.has_value()) return;

                // set root mac
                state::setRootMac(p.root.value());
                // set state
                state::setState(state::State::REACHES_ROOT);
                break;
            }
        }

        return;
    }

    // is direct child?
    auto child = std::find_if(layout.getChildren().begin(), layout.getChildren().end(),
                              [&](const layout::Child& child) { return child.mac == from; });
    if (child == layout.getChildren().end()) return;
    child->last_seen = xTaskGetTickCount();
}

void PacketHandler::handle(const util::MacAddr& from, const packets::SearchProbe& p) {
    if (!reachesRoot()) return;
    if (knowsNode(from)) return;
    if (!canAcceptNewChild()) return;

    // send reply
    ESP_LOGV(TAG, "Sending I Am Here");
    send::enqueuePayload(packets::SearchReply{}, send::SendBehavior::directSingleTry(from), true);
}

void PacketHandler::handle(const util::MacAddr& from, int rssi, const packets::SearchReply&) {
    if (!disconnected()) return;
    if (knowsNode(from)) return;

    // fire event to let connect job know
    auto mac = new util::MacAddr(from);
    event::ParentFoundData data{
        .mac = mac,
        .rssi = rssi,
    };
    event::fireEvent(event::MESHNOW_INTERNAL, event::InternalEvent::PARENT_FOUND, &data, sizeof(data));
    delete mac;
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ConnectRequest& p) {
    if (!reachesRoot()) return;
    if (knowsNode(from)) return;
    if (!canAcceptNewChild()) return;

    // add to layout
    layout().addChild(from);

    // send reply
    ESP_LOGI(TAG, "Sending Connect Response");
    send::enqueuePayload(packets::ConnectOk{state::getRootMac()}, send::SendBehavior::directSingleTry(from), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ConnectOk& p) {
    if (!disconnected()) return;
    if (knowsNode(from)) return;

    // fire event to let connect job know
    auto parent_mac = new util::MacAddr(from);
    auto root_mac = new util::MacAddr(p.root);
    event::GotConnectResponseData data{
        .mac = parent_mac,
        .root_mac = root_mac,
    };
    event::fireEvent(event::MESHNOW_INTERNAL, event::InternalEvent::GOT_CONNECT_RESPONSE, &data, sizeof(data));
    delete parent_mac;
    delete root_mac;
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ResetRequest& p) {
    if (isParent(from)) return;
    if (!isChild(from)) return;

    auto& layout = layout::Layout::get();

    // go through every child first
    for (auto& child : layout.getChildren()) {
        // if the child is the one that sent the reset request, reset its sequence number
        if (child.mac == from) {
            child.seq = 0;
            break;
        }

        // otherwise check the routing table and reset there if necessary
        for (auto& entry : child.routing_table) {
            if (entry.mac == from) {
                entry.seq = 0;
                break;
            }
        }
    }

    // forward upstream
    if (layout.getParent()) {
        send::enqueuePayload(p, send::SendBehavior::parent(), true);
    }

    // if root we answer with an OK
    if (state::isRoot()) {
        send::enqueuePayload(packets::ResetOk{p.id, p.from}, send::SendBehavior::resolve(p.from, state::getThisMac()),
                             true);
    }
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ResetOk& p) {
    if (!isParent(from)) return;
    if (isChild(from)) return;

    // if we are the target then fire the corresponding event
    if (p.to == state::getThisMac()) {
        event::GotResetOk event{p.id};
        event::fireEvent(event::MESHNOW_INTERNAL, event::InternalEvent::GOT_RESET_OK, &event, sizeof(event));
    }
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RemoveFromRoutingTable& p) {
    if (isParent(from)) return;
    if (!isChild(from)) return;

    auto& layout = layout::Layout::get();

    // remove from routing table
    for (auto& child : layout.getChildren()) {
        for (auto entry = child.routing_table.begin(); entry != child.routing_table.end(); ++entry) {
            if (p.to_remove == entry->mac) {
                child.routing_table.erase(entry);
                break;
            }
        }
    }

    // forward upstream
    if (layout.getParent()) {
        send::enqueuePayload(p, send::SendBehavior::parent(), true);
    }
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootUnreachable& p) {
    if (!reachesRoot()) return;
    if (!isParent(from)) return;
    if (isChild(from)) return;

    ESP_LOGI(TAG, "Got Root Unreachable packet from parent");

    state::setState(state::State::CONNECTED_TO_PARENT);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootReachable& p) {
    if (!isParent(from)) return;
    if (state::getState() != state::State::CONNECTED_TO_PARENT) return;

    state::setState(state::State::REACHES_ROOT);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::DataFragment& p) {
    if (!isNeighbor(from)) return;

    bool forward{false};
    bool consume{false};

    if (p.to.isBroadcast()) {
        // in case of broadcast we want it, but also forward it
        forward = consume = true;
    } else if ((p.to.isRoot() && state::isRoot()) || p.to == state::getThisMac()) {
        // if directed to this node, only consume
        consume = true;
    } else {
        // otherwise the packet is for some other node
        forward = true;
    }

    if (consume) {
        fragments::addFragment(p.from, p.id, p.frag_num, p.total_size, p.data);
    }

    if (forward) {
        send::enqueuePayload(p, send::SendBehavior::resolve(p.to, from), true);
    }
}

}  // namespace meshnow::job