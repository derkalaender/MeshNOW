#pragma once

#include <esp_netif.h>
#include <esp_wifi.h>

#include <memory>

#include "send/worker.hpp"

namespace meshnow {

class NowNetif {
   public:
    esp_err_t init();

    void deinit();

    void start();

    void stop();

   private:
    struct NetifDeleter {
        void operator()(esp_netif_t* netif) const { esp_netif_destroy(netif); }
    };

    using netif_ptr = std::unique_ptr<esp_netif_t, NetifDeleter>;

    static netif_ptr createInterface();

    esp_err_t setMac();

    esp_err_t initRootSpecific();

    void deinitRootSpecific();

    [[noreturn]] void io_receive_task();

    netif_ptr netif_;

    std::unique_ptr<esp_netif_driver_base_t> io_driver_;
};

}  // namespace meshnow