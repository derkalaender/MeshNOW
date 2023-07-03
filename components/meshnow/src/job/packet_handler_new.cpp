#include "packet_handler_new.hpp"

#include <esp_log.h>

#include "event_internal.hpp"
#include "fragments.hpp"
#include "layout.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/lock.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("PacketHandler");

void PacketHandler::handlePacket(const util::MacAddr& from, const packets::Packet& packet) {
    // TODO handle duplicate packets

    auto payload = packet.payload;

    // simply visit the corresponding overload
    std::visit([&](const auto& p) { handle(from, p); }, payload);
}

// HANDLERS //

void PacketHandler::handle(const util::MacAddr& from, const packets::Status& p) {
    util::Lock lock{routing::getMtx()};
    auto& layout = routing::getLayout();

    // is parent?
    if (layout.parent && layout.parent->mac == from) {
        // got status from parent
        layout.parent->last_seen = xTaskGetTickCount();
        switch (p.state) {
            case state::State::DISCONNECTED_FROM_PARENT:
            case state::State::CONNECTED_TO_PARENT: {
                // TODO event handler for this that then sends the root unreachable packet
                state::setState(state::State::CONNECTED_TO_PARENT);
                break;
            }
            case state::State::REACHES_ROOT: {
                assert(p.root_mac.has_value());
                // set root mac
                state::setRootMac(p.root_mac.value());
                // set state
                // TODO event handler for this that then sends the root reachable packet
                state::setState(state::State::REACHES_ROOT);
                break;
            }
        }

        return;
    }

    // is direct child?
    auto it = routing::getDirectChild(from);

    if (it == layout.children.end()) return;

    // got status from child
    it->last_seen = xTaskGetTickCount();
}

void PacketHandler::handle(const util::MacAddr& from, const packets::AnyoneThere& p) {
    // TODO check if we reached the maximum number of children
    // only offer connection if we have a parent and can reach the root -> disconnected islands won't grow
    if (state::getState() != state::State::REACHES_ROOT) return;

    {
        util::Lock lock{routing::getMtx()};
        // we must have a parent by this point
        assert(routing::getLayout().parent.has_value());

        // check if node is already present in layout
        // should prevent most loops from forming
        if (routing::has(from)) return;
    }

    // send reply
    ESP_LOGI(TAG, "Sending I Am Here");
    send::enqueuePayload(packets::IAmHere{}, send::SendBehavior::direct(from), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::IAmHere&) {
    // only process if we are disconnected
    if (state::getState() != state::State::DISCONNECTED_FROM_PARENT) return;

    {
        util::Lock lock(routing::getMtx());
        const auto layout = routing::getLayout();

        // ignore if we're already connected to a parent
        if (layout.parent.has_value()) return;

        // ignore if child
        if (routing::has(from)) return;
    }

    // fire event to let connect job know
    // TODO GET ACTUAL RSSI VALUE
    auto mac = new util::MacAddr(from);
    event::ParentFoundData data{
        .mac = mac,
        .rssi = 0,
    };
    event::fireEvent(event::MESHNOW_INTERNAL, event::InternalEvent::PARENT_FOUND, &data, sizeof(data));
    delete mac;
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ConnectRequest& p) {
    // TODO check if we reached the maximum number of children
    // only offer connection if we have a parent and can reach the root -> disconnected islands won't grow
    if (state::getState() != state::State::REACHES_ROOT) return;

    {
        util::Lock lock{routing::getMtx()};
        // we must have a parent by this point
        assert(routing::getLayout().parent.has_value());

        // check if node is already present in layout
        // should prevent most loops from forming
        if (routing::has(from)) return;

        // add to layout
        routing::addDirectChild(from);

        // send node connected upstream
        send::enqueuePayload(packets::NodeConnected{.child_mac = from}, send::SendBehavior::parent(), true);
    }

    // send reply
    ESP_LOGI(TAG, "Sending Connect Response");
    send::enqueuePayload(packets::ConnectResponse{}, send::SendBehavior::direct(from), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ConnectResponse& p) {
    // only process if we are disconnected
    if (state::getState() != state::State::DISCONNECTED_FROM_PARENT) return;

    {
        util::Lock lock(routing::getMtx());
        const auto layout = routing::getLayout();

        // ignore if we're already connected to a parent
        if (layout.parent.has_value()) return;

        // ignore if child
        if (routing::has(from)) return;
    }

    // fire event to let connect job know
    // TODO GET ACTUAL RSSI VALUE
    auto mac = new util::MacAddr(from);
    event::GotConnectResponseData data{
        .mac = mac,
    };
    event::fireEvent(event::MESHNOW_INTERNAL, event::InternalEvent::GOT_CONNECT_RESPONSE, &data, sizeof(data));
    delete mac;
}

void PacketHandler::handle(const util::MacAddr& from, const packets::NodeConnected& p) {
    {
        util::Lock lock(routing::getMtx());
        const auto layout = routing::getLayout();

        // ignore if packet wasn't sent by a child
        if (!routing::hasDirectChild(from)) return;

        // ignore if from parent, should never happen
        if (p.child_mac == layout.parent->mac) return;

        // ignore if parent mac is not one of the (indirect) children
        if (!routing::has(p.parent_mac)) return;

        // add to layout
        routing::addIndirectChild(p.parent_mac, p.child_mac);

        // TODO reset sequence number

        // continue if we're connected to a parent
        if (!layout.parent.has_value()) return;
    }

    // forward upstream
    send::enqueuePayload(p, send::SendBehavior::parent(), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::NodeDisconnected& p) {
    {
        util::Lock lock(routing::getMtx());
        const auto layout = routing::getLayout();

        // ignore if packet wasn't sent by a child
        if (!routing::hasDirectChild(from)) return;

        // ignore if from parent, should never happen
        if (p.child_mac == layout.parent->mac) return;

        // ignore if parent mac is not one of the (indirect) children
        if (!routing::has(p.parent_mac)) return;

        // remove from layout
        routing::removeIndirectChild(p.parent_mac, p.child_mac);

        // continue if we're connected to a parent
        if (!layout.parent.has_value()) return;
    }

    // forward upstream
    send::enqueuePayload(p, send::SendBehavior::parent(), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootUnreachable& p) {
    if (state::getState() != state::State::REACHES_ROOT) return;
    {
        util::Lock lock{routing::getMtx()};
        const auto& parent = routing::getLayout().parent;
        // we must have a parent by this point
        assert(parent.has_value());
        if (parent->mac != from) return;
    }

    // TODO event handler
    state::setState(state::State::CONNECTED_TO_PARENT);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootReachable& p) {
    if (state::getState() != state::State::CONNECTED_TO_PARENT) return;
    {
        util::Lock lock{routing::getMtx()};
        const auto& parent = routing::getLayout().parent;
        // we must have a parent by this point
        assert(parent.has_value());
        if (parent->mac != from) return;
    }

    // TODO event handler
    state::setState(state::State::REACHES_ROOT);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::DataFragment& p) {
    {
        // check if from a neighbor
        util::Lock lock{routing::getMtx()};
        if (!routing::has(from)) return;
    }

    bool forward{false};
    bool consume{false};

    if (p.target.isBroadcast()) {
        // in case of broadcast we want it, but also forward it
        forward = consume = true;
    } else if ((p.target.isRoot() && state::isRoot()) || p.target == state::getThisMac()) {
        // if directed to this node, only consume
        consume = true;
    } else {
        // otherwise the packet is for some other node
        forward = true;
    }

    if (consume) {
        fragments::addFragment(p.source, p.id, p.frag_num, p.total_size, p.data);
    }

    if (forward) {
        send::enqueuePayload(p, send::SendBehavior::resolve(), true);
    }
}

}  // namespace meshnow::job