#pragma once

#include <map>
#include <variant>
#include <vector>

#include "constants.hpp"
#include "hand_shaker.hpp"
#include "keep_alive.hpp"
#include "layout.hpp"
#include "packets.hpp"
#include "receive_meta.hpp"
#include "send_worker.hpp"

namespace meshnow {

/**
 * Handles incoming packets.
 */
class PacketHandler {
   public:
    PacketHandler(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<NodeState> state,
                  std::shared_ptr<routing::Layout> layout, HandShaker& hand_shaker,
                  keepalive::NeighborsAliveCheckTask& neighbors_alive_check_task,
                  meshnow::keepalive::RootReachableCheckTask& rootReachableCheckTask);

    /**
     * Handle a packet. Calls the corresponding private methods.
     * @param meta the meta data of the packet
     * @param p the payload of the packet
     */
    void handlePacket(const ReceiveMeta& meta, const packets::Payload& p);

   private:
    /**
     * Update the RSSI of a neighbor.
     * @param meta the meta data of the packet
     */
    void updateRssi(const meshnow::ReceiveMeta& meta);

    // HANDLERS for each payload type //

    void handle(const ReceiveMeta& meta, const packets::KeepAlive& p);
    void handle(const ReceiveMeta& meta, const packets::AnyoneThere& p);
    void handle(const ReceiveMeta& meta, const packets::IAmHere& p);
    void handle(const ReceiveMeta& meta, const packets::PlsConnect& p);
    void handle(const ReceiveMeta& meta, const packets::Verdict& p);
    void handle(const ReceiveMeta& meta, const packets::NodeConnected& p);
    void handle(const ReceiveMeta& meta, const packets::NodeDisconnected& p);
    void handle(const ReceiveMeta& meta, const packets::RootUnreachable& p);
    void handle(const ReceiveMeta& meta, const packets::RootReachable& p);
    void handle(const ReceiveMeta& meta, const packets::Ack& p);
    void handle(const ReceiveMeta& meta, const packets::Nack& p);
    void handle(const ReceiveMeta& meta, const packets::LwipDataFirst& p);
    void handle(const ReceiveMeta& meta, const packets::CustomDataFirst& p);
    void handle(const ReceiveMeta& meta, const packets::LwipDataNext& p);
    void handle(const ReceiveMeta& meta, const packets::CustomDataNext& p);

    std::shared_ptr<SendWorker> send_worker_;
    std::shared_ptr<NodeState> state_;
    std::shared_ptr<routing::Layout> layout_;
    HandShaker& hand_shaker_;
    keepalive::NeighborsAliveCheckTask& neighbors_alive_check_task_;
    keepalive::RootReachableCheckTask& root_reachable_check_task_;

    std::map<MAC_ADDR, uint32_t> last_id;
};

}  // namespace meshnow