#pragma once

#include <esp_netif.h>
#include <esp_wifi.h>

#include <memory>

#include "now_lwip/io.hpp"

namespace meshnow::lwip::netif {

// TODO Root also needs the default station netif.
// this is created automatically, but we should check and create it if missing

// TODO stop functions

class Netif {
   public:
    virtual ~Netif() = default;

    /**
     * Initialize the network interface.
     */
    void init();

    /**
     * Start the network interface.
     */
    virtual void start() = 0;

   protected:
    /**
     * Creates the underlying esp netif object and returns a pointer to it
     */
    virtual esp_netif_t* createInterface() = 0;

    /**
     * Creates the IODriver that is then attached to the network interface.
     */
    std::unique_ptr<io::IODriver> createIODriver();

    /**
     * Copy the wifi interface mac address to the custom netif.
     */
    void setNetifMac(wifi_interface_t wifi_interface);

    /**
     * Unique pointer to the interface created by createInterface(). Allows easy destruction.
     */
    std::unique_ptr<esp_netif_t, decltype(&esp_netif_destroy)> netif_{nullptr, esp_netif_destroy};

    /**
     * IODriver with post-attach callback.
     */
    std::unique_ptr<io::IODriver> io_driver_;
};

class RootNetif : public Netif {
   public:
    void start() override;

   private:
    // use cloudflare 1.1.1.1 as DNS server
    // TODO temporary?
    static constexpr auto DNS_IP_ADDR = ESP_IP4TOADDR(1, 1, 1, 1);

    const esp_netif_ip_info_t subnet_ip = {
        .ip = {.addr = ESP_IP4TOADDR(10, 0, 0, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 0, 0)},
        .gw = {.addr = ESP_IP4TOADDR(10, 0, 0, 1)},
    };

    void set_dhcp_dns();

    esp_netif_t* createInterface() override;
};

class NodeNetif : public Netif {
   public:
    void start() override;

   private:
    esp_netif_t* createInterface() override;
};

}  // namespace meshnow::lwip::netif