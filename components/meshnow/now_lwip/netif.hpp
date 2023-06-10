#pragma once

#include <esp_netif.h>

#include <memory>

namespace meshnow::lwip::netif {

class Netif {
   public:
    void init();
    virtual void start() = 0;

   protected:
    virtual esp_netif_t* createInterface() = 0;
    std::unique_ptr<esp_netif_t, decltype(&esp_netif_destroy)> netif_{nullptr, esp_netif_destroy};
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