#pragma once

#include <freertos/portmacro.h>

#include "constants.hpp"
#include "router.hpp"
#include "send_worker.hpp"
#include "state.hpp"

namespace meshnow {

class KeepAlive {
   public:
    KeepAlive(SendWorker& send_worker, NodeState& state, routing::Router& router)
        : send_worker_{send_worker}, state_{state}, router_{router} {}

    /**
     * Checks if any neighbors have timed out and removes them if they have.
     */
    void checkConnections();

    /**
     * Sends a keep alive beacon to all neighbors if enough time has elapsed.
     */
    void sendKeepAliveBeacon();

    /**
     * @return the time at which the next action should be performed
     */
    TickType_t nextActionIn(TickType_t now) const;

    /**
     * A keep alive beacon was received from a neighbor.
     * @param mac_addr the MAC address of the neighbor
     */
    void receivedKeepAliveBeacon(const MAC_ADDR& mac_addr);

   private:
    SendWorker& send_worker_;
    NodeState& state_;
    routing::Router& router_;

    TickType_t last_beacon_sent_{0};
};

}  // namespace meshnow