#pragma once

#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "constants.hpp"
#include "packet_handler.hpp"
#include "receive_meta.hpp"
#include "send_worker.hpp"
#include "state.hpp"
#include "routing.hpp"

namespace meshnow {

/**
 * Whenever disconnected from a parent, this thread tries to connect to the best parent by continuously sending connect
 * requests.
 */
class Handshaker : public PacketHandlerTrait<Handshaker> {
   public:
    explicit Handshaker(SendWorker& send_worker, NodeState& state, routing::RoutingInfo& routing_info);

    /**
     * If this node can reach the root (i.e., is not part of island) and other conditions are met, it will reply with IAmHere.
     */
    bool handle(const ReceiveMeta& meta, const packets::AnyoneThere& p);

    /**
     * Adds the node as a potential parent.
     */
    bool handle(const ReceiveMeta& meta, const packets::IAmHere& p);

    /**
     * If this node can accept any more children will reply with an accepting verdict, otherwise reject the connecting node.
     */
    bool handle(const ReceiveMeta& meta, const packets::PlsConnect& p);

    /**
     * If accepted by the parent, this node will save it as its parent and send out events. Otherwise, will try the next best parent.
     */
    bool handle(const ReceiveMeta& meta, const packets::Verdict& p);

    /**
     * Notify the Handshaker that a possible parent was found.
     * @param mac_addr the MAC address of the parent
     * @param rssi rssi of the parent
     */
    void addPotentialParent(const meshnow::MAC_ADDR& mac_addr, int rssi);

   private:
    struct ParentInfo {
        MAC_ADDR mac_addr;
        int rssi;
    };

    [[noreturn]] void run();

    void sendBeacon();

    bool tryConnect();

    /**
     * Reject a possible parent.
     * @param mac_addr the MAC address of the parent
     */
    void reject(MAC_ADDR mac_addr);

    SendWorker& send_worker_;

    NodeState& state_;

    routing::RoutingInfo& routing_info_;

    std::thread thread_;

    struct {
        bool searching{true};
        util::WaitBits waitbits{};
        std::mutex mtx{};
    } sync_;

    // TODO place magic constant in internal.hpp
    std::vector<ParentInfo> parent_infos_{};

    TickType_t first_parent_found_time_{0};
};

}  // namespace meshnow