#pragma once

#include <esp_event.h>
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
    /**
     * Sends status beacons to all neighborsSingleTry.
     */
    static void sendStatus();

    TickType_t last_status_sent_{0};
};

class UnreachableTimeoutJob : public meshnow::job::Job {
   public:
    static void event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    UnreachableTimeoutJob();
    ~UnreachableTimeoutJob() override;

    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

   private:
    esp_event_handler_instance_t event_handler_instance_{nullptr};
    TickType_t mesh_unreachable_since_{0};
    bool awaiting_reachable{false};
};

class NeighborCheckJob : public meshnow::job::Job {
   public:
    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

   private:
    /**
     * Sends a child disconnected message upstream if possible.
     * @param mac_addr the MAC address of the child that disconnected
     */
    static void sendChildDisconnected(const util::MacAddr& mac);

    /**
     * Sends a root unreachable message downstream to all children if possible.
     */
    static void sendRootUnreachable();
};
}  // namespace meshnow::job