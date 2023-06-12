
#include "packet_handler.hpp"

#include <esp_log.h>

#include <mutex>

#include "internal.hpp"

static const char* TAG = CREATE_TAG("PacketHandler");

using meshnow::PacketHandler;

PacketHandler::PacketHandler(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<NodeState> state,
                             std::shared_ptr<routing::Layout> layout, HandShaker& hand_shaker,
                             keepalive::NeighborsAliveCheckTask& neighbors_alive_check_task,
                             keepalive::RootReachableCheckTask& rootReachableCheckTask,
                             fragment::FragmentTask& fragment_task)
    : send_worker_(std::move(send_worker)),
      state_(std::move(state)),
      layout_(std::move(layout)),
      hand_shaker_(hand_shaker),
      neighbors_alive_check_task_(neighbors_alive_check_task),
      root_reachable_check_task_(rootReachableCheckTask),
      fragment_task_(fragment_task) {}

void PacketHandler::updateRssi(const meshnow::ReceiveMeta& meta) {
    std::scoped_lock lock{layout_->mtx};
    auto neighbors = getNeighbors(layout_);
    for (auto& n : neighbors) {
        if (n->mac == meta.src_addr) {
            n->rssi = meta.rssi;
            break;
        }
    }
}

bool PacketHandler::isForMe(const MAC_ADDR& dest_addr) {
    std::scoped_lock lock{layout_->mtx};
    return dest_addr == layout_->mac || dest_addr == meshnow::BROADCAST_MAC_ADDR ||
           (dest_addr == meshnow::ROOT_MAC_ADDR && state_->isRoot());
}

void PacketHandler::handlePacket(const meshnow::ReceiveMeta& meta, const packets::Payload& p) {
    updateRssi(meta);

    // handle duplicate packets
    auto it = last_id.find(meta.src_addr);
    if (it != last_id.end()) {
        if (it->second == meta.id) {
            ESP_LOGW(TAG, "Duplicate packet, ignoring");
            return;
        }
    }
    last_id[meta.src_addr] = meta.id;

    // simply visit the corresponding overload
    std::visit([&](const auto& p) { handle(meta, p); }, p);
}

// HANDLERS //

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::KeepAlive& p) {
    neighbors_alive_check_task_.receivedKeepAliveBeacon(meta.src_addr);
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::AnyoneThere&) {
    hand_shaker_.receivedSearchProbe(meta.src_addr);
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::IAmHere&) {
    hand_shaker_.foundPotentialParent(meta.src_addr, meta.rssi);
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::PlsConnect&) {
    hand_shaker_.receivedConnectRequest(meta.src_addr);
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::Verdict& p) {
    hand_shaker_.receivedConnectResponse(meta.src_addr, p.accept, p.root_mac);
}

void PacketHandler::handle(const meshnow::ReceiveMeta&, const packets::NodeConnected& p) {
    std::scoped_lock lock(layout_->mtx);

    // add node
    insertChild(layout_, p.child_mac, p.parent_mac);
    // forward upwards
    if (!state_->isRoot()) {
        send_worker_->enqueuePayload(meshnow::ROOT_MAC_ADDR, true, p, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void PacketHandler::handle(const meshnow::ReceiveMeta&, const packets::NodeDisconnected& p) {
    std::scoped_lock lock(layout_->mtx);

    // remove from routing
    removeDirectChild(layout_, p.child_mac);

    if (!state_->isRoot()) {
        send_worker_->enqueuePayload(meshnow::ROOT_MAC_ADDR, true, p, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void PacketHandler::handle(const meshnow::ReceiveMeta&, const packets::RootUnreachable&) {
    root_reachable_check_task_.receivedRootUnreachable();

    std::scoped_lock lock(layout_->mtx);
    // forward to all children
    for (auto&& c : layout_->children) {
        send_worker_->enqueuePayload(c->mac, true, packets::RootUnreachable{}, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void PacketHandler::handle(const meshnow::ReceiveMeta&, const packets::RootReachable&) {
    root_reachable_check_task_.receivedRootReachable();

    std::scoped_lock lock(layout_->mtx);
    // forward to all children
    for (auto&& c : layout_->children) {
        send_worker_->enqueuePayload(c->mac, true, packets::RootReachable{}, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::Ack& p) {
    // TODO check if for this node, otherwise forward
    send_worker_->receivedAck(p.id_ack);
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::Nack& p) {
    // TODO check if for this node, otherwise forward
    send_worker_->receivedNack(p.id_nack, p.reason);
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::LwipDataFirst& p) {
    if (isForMe(p.target)) {
        fragment_task_.newFragmentFirst(p.source, p.id, p.size, p.data);
    } else {
        // forward
        send_worker_->enqueuePayload(p.target, true, p, SendPromise{}, false, QoS::NEXT_HOP);
    }
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::CustomDataFirst& p) {
    // TODO
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::LwipDataNext& p) {
    if (isForMe(p.target)) {
        fragment_task_.newFragmentNext(p.source, p.id, p.frag_num, p.data);
    } else {
        // forward
        send_worker_->enqueuePayload(p.target, true, p, SendPromise{}, false, QoS::NEXT_HOP);
    }
}

void PacketHandler::handle(const meshnow::ReceiveMeta& meta, const packets::CustomDataNext& p) {
    // TODO
}
