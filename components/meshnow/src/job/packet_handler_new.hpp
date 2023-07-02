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
    void handlePacket(const util::MacAddr& from, const packets::Packet& packet);

   private:
    // HANDLERS for each payload type //

    void handle(const util::MacAddr& from, const packets::Status& p);
    void handle(const util::MacAddr& from, const packets::AnyoneThere& p);
    void handle(const util::MacAddr& from, const packets::IAmHere& p);
    void handle(const util::MacAddr& from, const packets::ConnectRequest& p);
    void handle(const util::MacAddr& from, const packets::ConnectResponse& p);
    void handle(const util::MacAddr& from, const packets::NodeConnected& p);
    void handle(const util::MacAddr& from, const packets::NodeDisconnected& p);
    void handle(const util::MacAddr& from, const packets::RootUnreachable& p);
    void handle(const util::MacAddr& from, const packets::RootReachable& p);
    void handle(const util::MacAddr& from, const packets::DataFragment& p);
};

}  // namespace meshnow::job