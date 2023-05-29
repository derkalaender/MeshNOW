
#include "packet_handler.hpp"

#include <esp_log.h>

#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("PacketHandler");

meshnow::packets::PacketHandler::PacketHandler(meshnow::Networking& networking) : net_{networking} {}

void meshnow::packets::PacketHandler::handlePacket(const meshnow::ReceiveMeta& meta,
                                                   const meshnow::packets::Payload& p) {
    // update RSSI
    net_.router_.updateRssi(meta.src_addr, meta.rssi);

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

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::KeepAlive& p) {
    net_.keep_alive_.receivedKeepAliveBeacon(meta.src_addr);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::AnyoneThere&) {
    net_.handshaker_.receivedSearchProbe(meta.src_addr);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::IAmHere&) {
    net_.handshaker_.foundPotentialParent(meta.src_addr, meta.rssi);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::PlsConnect&) {
    net_.handshaker_.receivedConnectRequest(meta.src_addr);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::Verdict& p) {
    net_.handshaker_.receivedConnectResponse(meta.src_addr, p.accept, p.root_mac);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta&, const meshnow::packets::NodeConnected& p) {
    // add node
    net_.router_.addChild(p.child_mac, p.parent_mac);
    // forward upwards
    if (!net_.state_.isRoot()) {
        net_.send_worker_.enqueuePayload(meshnow::ROOT_MAC_ADDR, true, p, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::NodeDisconnected& p) {
    // stop tracking
    net_.keep_alive_.stopTrackingNeighbor(p.child_mac);
    // remove from routing
    net_.router_.removeChild(p.child_mac);

    if (!net_.state_.isRoot()) {
        net_.send_worker_.enqueuePayload(meshnow::ROOT_MAC_ADDR, true, p, SendPromise{}, true, QoS::NEXT_HOP);
    }
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::MeshUnreachable& p) {
    // TODO
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::MeshReachable& p) {
    // TODO
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::Ack& p) {
    // TODO check if for this node, otherwise forward
    net_.send_worker_.receivedAck(p.id_ack);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::Nack& p) {
    // TODO check if for this node, otherwise forward
    net_.send_worker_.receivedNack(p.id_nack, p.reason);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::LwipDataFirst& p) {
    // TODO
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::CustomDataFirst& p) {
    // TODO
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::LwipDataNext& p) {
    // TODO
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::CustomDataNext& p) {
    // TODO
}
