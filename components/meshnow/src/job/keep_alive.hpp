#pragma once

#include <freertos/portmacro.h>

#include <memory>

#include "constants.hpp"
#include "job.hpp"
#include "job/job.hpp"
#include "layout.hpp"
#include "now_lwip/netif.hpp"
#include "send/worker.hpp"
#include "state.hpp"

namespace meshnow::keepalive {

class BeaconSendTask : public meshnow::job::Job {
   public:
    BeaconSendTask(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<routing::Layout> layout);

    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

   private:
    std::shared_ptr<SendWorker> send_worker_;
    std::shared_ptr<routing::Layout> layout_;

    TickType_t last_beacon_sent_{0};
};

class RootReachableCheckTask : public meshnow::job::Job {
   public:
    RootReachableCheckTask(std::shared_ptr<NodeState> state, std::shared_ptr<routing::Layout> layout,
                           std::shared_ptr<lwip::netif::Netif> netif);

    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

    /**
     * A mesh unreachable message was received. Starts parent disconnect timeout.
     */
    void receivedRootUnreachable();

    /**
     * A mesh reachable message was received. Stops parent disconnect timeout.
     */
    void receivedRootReachable();

   private:
    std::shared_ptr<NodeState> state_;
    std::shared_ptr<routing::Layout> layout_;
    std::shared_ptr<lwip::netif::Netif> netif_;

    TickType_t mesh_unreachable_since_{0};
    bool awaiting_reachable{false};
};

class NeighborsAliveCheckTask : public meshnow::job::Job {
   public:
    NeighborsAliveCheckTask(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<NodeState> state,
                            std::shared_ptr<routing::Layout> layout, std::shared_ptr<lwip::netif::Netif> netif);

    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

    /**
     * A keep alive beacon was received from a neighbor.
     * @param from_mac the MAC address of the neighbor
     */
    void receivedKeepAliveBeacon(const MAC_ADDR& from_mac);

   private:
    /**
     * Sends a child disconnected message to the parent.
     * @param mac_addr the MAC address of the child that disconnected
     */
    void sendChildDisconnected(const MAC_ADDR& mac_addr);

    /**
     * Sends a root unreachable message to all neighbors.
     */
    void sendRootUnreachable();

    std::shared_ptr<SendWorker> send_worker_;
    std::shared_ptr<NodeState> state_;
    std::shared_ptr<routing::Layout> layout_;
    std::shared_ptr<lwip::netif::Netif> netif_;
};
}  // namespace meshnow::keepalive