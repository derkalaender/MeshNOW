#pragma once

#include <variant>
#include <vector>

#include "packets.hpp"
#include "receive_meta.hpp"

namespace meshnow {

// Forward declare the Networking class. Will do everything through it.
class Networking;

namespace packets {

/**
 * Handles incoming packets.
 */
class PacketHandler {
   public:
    explicit PacketHandler(Networking& networking);

    /**
     * Handle a packet. Calls the corresponding private methods.
     * @param meta the meta data of the packet
     * @param p the payload of the packet
     */
    void handlePacket(const ReceiveMeta& meta, const Payload& p);

   private:
    // HANDLERS for each payload type //

    void handle(const ReceiveMeta& meta, const StillAlive& p);
    void handle(const ReceiveMeta& meta, const AnyoneThere& p);
    void handle(const ReceiveMeta& meta, const IAmHere& p);
    void handle(const ReceiveMeta& meta, const PlsConnect& p);
    void handle(const ReceiveMeta& meta, const Verdict& p);
    void handle(const ReceiveMeta& meta, const NodeConnected& p);
    void handle(const ReceiveMeta& meta, const NodeDisconnected& p);
    void handle(const ReceiveMeta& meta, const MeshUnreachable& p);
    void handle(const ReceiveMeta& meta, const MeshReachable& p);
    void handle(const ReceiveMeta& meta, const Ack& p);
    void handle(const ReceiveMeta& meta, const Nack& p);
    void handle(const ReceiveMeta& meta, const LwipDataFirst& p);
    void handle(const ReceiveMeta& meta, const CustomDataFirst& p);
    void handle(const ReceiveMeta& meta, const LwipDataNext& p);
    void handle(const ReceiveMeta& meta, const CustomDataNext& p);

    Networking& net_;
};

}  // namespace packets
}  // namespace meshnow