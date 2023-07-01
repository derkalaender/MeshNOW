#pragma once

#include <esp_err.h>

#include <memory>

#include "receive/worker.hpp"
#include "util/task.hpp"
#include "util/waitbits.hpp"

namespace meshnow {

class Networking {
   public:
    Networking() = default;

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    esp_err_t init();

    void deinit();

    /**
     * Starts the networking stack.
     */
    esp_err_t start();

    /**
     * Stops the networking stack.
     */
    void stop();

   private:
    util::WaitBits task_waitbits_;
    bool stop_tasks_{false};
    util::Task job_runner_task_;
    util::Task send_worker_task_;

    //    std::unique_ptr<lwip::netif::Netif> netif_;
};

}  // namespace meshnow
