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
    static void handlePacket(const util::MacAddr& from, const packets::Packet& packet);

   private:
    // HANDLERS for each payload type //

    static void handle(const util::MacAddr& from, const packets::Status& p);
    static void handle(const util::MacAddr& from, const packets::AnyoneThere& p);
    static void handle(const util::MacAddr& from, const packets::IAmHere& p);
    static void handle(const util::MacAddr& from, const packets::ConnectRequest& p);
    static void handle(const util::MacAddr& from, const packets::ConnectResponse& p);
    static void handle(const util::MacAddr& from, const packets::NodeConnected& p);
    static void handle(const util::MacAddr& from, const packets::NodeDisconnected& p);
    static void handle(const util::MacAddr& from, const packets::RootUnreachable& p);
    static void handle(const util::MacAddr& from, const packets::RootReachable& p);
    static void handle(const util::MacAddr& from, const packets::DataFragment& p);
};

}  // namespace meshnow::job