#pragma once

#include <esp_netif.h>
#include <esp_wifi.h>

#include <memory>
#include <optional>

#include "send/worker.hpp"
#include "util/event.hpp"

namespace meshnow {

class NowNetif {
   public:
    struct NetifDeleter {
        void operator()(esp_netif_t* netif) const { esp_netif_destroy(netif); }
    };

    using netif_ptr = std::unique_ptr<esp_netif_t, NetifDeleter>;

    esp_err_t init();

    void deinit();

    void start();

    void stop();

    netif_ptr netif_;

   private:
    static netif_ptr createInterface();

    esp_err_t setMac();

    esp_err_t initRootSpecific();

    void deinitRootSpecific();

    [[noreturn]] void io_receive_task();

    std::unique_ptr<esp_netif_driver_base_t> io_driver_;

    // event, we only want this when started
    std::optional<util::EventHandlerInstance> event_handler_instance_;
};

}  // namespace meshnow