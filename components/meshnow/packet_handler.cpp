#include "packet_handler.hpp"

#include "networking.hpp"

meshnow::packets::PacketHandler::PacketHandler(meshnow::Networking& networking) : net_{networking} {}

void meshnow::packets::PacketHandler::handlePacket(const meshnow::ReceiveMeta& meta,
                                                   const meshnow::packets::Payload& p) {
    // update RSSI
    net_.router_.updateRssi(meta.src_addr, meta.rssi);

    // TODO handle sequence numbers

    // simply visit the corresponding overload
    std::visit([&](const auto& p) { handle(meta, p); }, p);
}

// HANDLERS //

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::StillAlive& p) {
    // TODO
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

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::NodeConnected& p) {
    // TODO
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta,
                                             const meshnow::packets::NodeDisconnected& p) {
    // TODO
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
    net_.send_worker_.receivedAck(p.seq_num_ack);
}

void meshnow::packets::PacketHandler::handle(const meshnow::ReceiveMeta& meta, const meshnow::packets::Nack& p) {
    // TODO check if for this node, otherwise forward
    net_.send_worker_.receivedNack(p.seq_num_nack, p.reason);
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
