#pragma once

#include "packets.hpp"

namespace meshnow::job {

/**
 * Handles incoming packets.
 */
class PacketHandler {
   public:
    /**
     * Handle a packet. Calls the corresponding private methods.
     * @param from the mac address of the sender
     * @param packet the packet to handle
     */
    static void handlePacket(const util::MacAddr& from, int rssi, const packets::Packet& packet);

   private:
    // HANDLERS for each payload type //

    static void handle(const util::MacAddr& from, const packets::Status& p);
    static void handle(const util::MacAddr& from, const packets::SearchProbe& p);
    static void handle(const util::MacAddr& from, int rssi, const packets::SearchReply& p);
    static void handle(const util::MacAddr& from, const packets::ConnectRequest& p);
    static void handle(const util::MacAddr& from, const packets::ConnectOk& p);
    static void handle(const util::MacAddr& from, const packets::ResetRequest& p);
    static void handle(const util::MacAddr& from, const packets::ResetOk& p);
    static void handle(const util::MacAddr& from, const packets::RemoveFromRoutingTable& p);
    static void handle(const util::MacAddr& from, const packets::RootUnreachable& p);
    static void handle(const util::MacAddr& from, const packets::RootReachable& p);
    static void handle(const util::MacAddr& from, const packets::DataFragment& p);
};

}  // namespace meshnow::job