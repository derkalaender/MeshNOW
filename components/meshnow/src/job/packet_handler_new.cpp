#include "packet_handler_new.hpp"

#include <esp_log.h>

#include "util/util.hpp"

namespace meshnow::job {

static const char* TAG = CREATE_TAG("PacketHandler");

void PacketHandler::handlePacket(const util::MacAddr& from, const packets::Packet& packet) {
    // TODO handle duplicate packets

    auto payload = packet.payload;

    // simply visit the corresponding overload
    std::visit([&](const auto& p) { handle(from, p); }, payload);
}

// HANDLERS //

void PacketHandler::handle(const util::MacAddr& from, const packets::KeepAlive& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::AnyoneThere& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::IAmHere& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::PlsConnect& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::Verdict& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::NodeConnected& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::NodeDisconnected& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootUnreachable& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::RootReachable& p) {}

void PacketHandler::handle(const util::MacAddr& from, const packets::DataFragment& p) {}

}  // namespace meshnow::job