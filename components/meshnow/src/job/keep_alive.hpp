#pragma once

#include <freertos/portmacro.h>

#include <memory>

#include "job.hpp"
#include "layout.hpp"
#include "now_lwip/netif.hpp"
#include "send/worker.hpp"
#include "state.hpp"

namespace meshnow::job {

class StatusSendJob : public meshnow::job::Job {
   public:
    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

   private:
    TickType_t last_status_sent_{0};
};

class UnreachableTimeoutJob : public meshnow::job::Job {
   public:
    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

   private:
    TickType_t mesh_unreachable_since_{0};
    bool awaiting_reachable{false};
};

class NeighborCheckJob : public meshnow::job::Job {
   public:
    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

   private:
    /**
     * Sends a child disconnected message to the parent.
     * @param mac_addr the MAC address of the child that disconnected
     */
    void sendChildDisconnected(const util::MacAddr& mac_addr);

    /**
     * Sends a root unreachable message to all neighbors.
     */
    void sendRootUnreachable();
};
}  // namespace meshnow::job