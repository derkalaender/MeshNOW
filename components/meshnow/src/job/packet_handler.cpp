#include "packet_handler.hpp"

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
    // TODO update routing table

    auto payload = packet.payload;

    // simply visit the corresponding overload
    std::visit([&](const auto& p) { handle(from, p); }, payload);
}

// HANDLERS //

void PacketHandler::handle(const util::MacAddr& from, const packets::Status& p) {
    util::Lock lock{layout::mtx()};
    auto& layout = layout::Layout::get();

    // is parent?
    if (auto parent = layout.getParent(); parent && parent->mac == from) {
        // got status from parent
        parent->last_seen = xTaskGetTickCount();
        switch (p.state) {
            case state::State::DISCONNECTED_FROM_PARENT:
            case state::State::CONNECTED_TO_PARENT: {
                state::setState(state::State::CONNECTED_TO_PARENT);
                break;
            }
            case state::State::REACHES_ROOT: {
                assert(p.root_mac.has_value());
                // set root mac
                state::setRootMac(p.root_mac.value());
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
    // TODO check if we reached the maximum number of children
    // only offer connection if we have a parent and can reach the root -> disconnected islands won't grow
    if (state::getState() != state::State::REACHES_ROOT) return;

    {
        util::Lock lock{layout::mtx()};
        auto& layout = layout::Layout::get();

        // we must have a parent by this point
        assert(layout.getParent());

        // check if node is already present in layout
        // should prevent most loops from forming
        if (layout.has(from)) return;
    }

    // send reply
    ESP_LOGI(TAG, "Sending I Am Here");
    send::enqueuePayload(packets::SearchReply{}, send::SendBehavior::direct(from), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::SearchReply&) {
    // only process if we are disconnected
    if (state::getState() != state::State::DISCONNECTED_FROM_PARENT) return;

    {
        util::Lock lock(layout::mtx());
        auto& layout = layout::Layout::get();

        // we must not have a parent by this point
        assert(!layout.getParent());

        // check if node is already present in layout
        // should prevent most loops from forming
        if (layout.has(from)) return;
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

    bool added{false};

    {
        util::Lock lock{layout::mtx()};
        auto& layout = layout::Layout::get();
        // we must have a parent by this point
        assert(layout.getParent());

        // check if node is already present in layout
        // should prevent most loops from forming
        if (layout.has(from)) return;

        // add to layout
        layout.addChild(from);
        added = true;
    }

    // send reply
    ESP_LOGI(TAG, "Sending Connect Response");
    send::enqueuePayload(
        packets::ConnectResponse{.accept = added,
                                 .root_mac = added ? std::make_optional(state::getRootMac()) : std::nullopt},
        send::SendBehavior::direct(from), true);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ConnectResponse& p) {
    // only process if we are disconnected
    if (state::getState() != state::State::DISCONNECTED_FROM_PARENT) return;

    {
        util::Lock lock(layout::mtx());
        auto& layout = layout::Layout::get();

        // we must not have a parent by this point
        assert(!layout.getParent());

        // check if node is already present in layout
        // should prevent most loops from forming
        if (layout.has(from)) return;
    }

    // fire event to let connect job know
    auto parent_mac = new util::MacAddr(from);
    auto root_mac = new std::optional(p.root_mac);
    event::GotConnectResponseData data{
        .mac = parent_mac,
        .root_mac = root_mac,
        .accepted = p.accept,
    };
    event::fireEvent(event::MESHNOW_INTERNAL, event::InternalEvent::GOT_CONNECT_RESPONSE, &data, sizeof(data));
    delete parent_mac;
    delete root_mac;
}

void PacketHandler::handle(const util::MacAddr& from, const packets::Reset& p) {
    util::Lock lock(layout::mtx());
    auto& layout = layout::Layout::get();

    // ignore if packet was sent by parent (should never happen)

    // forward upstream
    if (layout.getParent()) {
        send::enqueuePayload(p, send::SendBehavior::parent(), true);
    }

    // TODO reset sequence number
    for (auto& child : layout.getChildren()) {
        //
    }
}

void PacketHandler::handle(const util::MacAddr& from, const packets::ResetOk& p) {
    // TODO
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RemoveFromRoutingTable& p) {
    util::Lock lock(layout::mtx());
    auto& layout = layout::Layout::get();

    // ignore if packet was sent by parent (should never happen)
    if (const auto& parent = layout.getParent(); parent && parent->mac == from) return;

    // forward upstream
    if (layout.getParent()) {
        send::enqueuePayload(p, send::SendBehavior::parent(), true);
    }

    // remove from routing table
    for (auto& child : layout.getChildren()) {
        for (auto entry = child.routing_table.begin(); entry != child.routing_table.end(); ++entry) {
            if (p.mac == entry->mac) {
                child.routing_table.erase(entry);
                break;
            }
        }
    }
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootUnreachable& p) {
    if (state::getState() != state::State::REACHES_ROOT) return;
    {
        util::Lock lock{layout::mtx()};
        const auto& parent = layout::Layout::get().getParent();
        // we must have a parent by this point
        assert(parent);
        if (parent->mac != from) return;
    }

    state::setState(state::State::CONNECTED_TO_PARENT);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootReachable& p) {
    if (state::getState() != state::State::CONNECTED_TO_PARENT) return;
    {
        util::Lock lock{layout::mtx()};
        const auto& parent = layout::Layout::get().getParent();
        // we must have a parent by this point
        assert(parent);
        if (parent->mac != from) return;
    }

    state::setState(state::State::REACHES_ROOT);
}

void PacketHandler::handle(const util::MacAddr& from, const packets::DataFragment& p) {
    {
        // check if from a neighbor
        util::Lock lock{layout::mtx()};
        const auto& layout = layout::Layout::get();
        if (!layout.has(from)) return;
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
        send::enqueuePayload(p, send::SendBehavior::resolve(p.target, from), true);
    }
}

}  // namespace meshnow::job