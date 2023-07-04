#include "netif.hpp"

#include <dhcpserver/dhcpserver.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <lwip/ip4_addr.h>
#include <lwip/lwip_napt.h>

#include <memory>

#include "io.hpp"
#include "state.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::lwip::netif {

static const char* TAG = CREATE_TAG("LWIP | Netif");

void Netif::init() {
    ESP_LOGI(TAG, "Initializing");
    netif_.reset(createInterface(), esp_netif_destroy);
    ESP_LOGI(TAG, "Initialized");
}

void Netif::start() {
    // TODO rx task
    // create IO driver
    ESP_LOGI(TAG, "Creating IO driver");
    io_driver_.driver_impl = createIODriverImpl();

    ESP_LOGI(TAG, "Attaching driver to network interface");
    esp_netif_attach(netif_.get(), &io_driver_);
    ESP_LOGI(TAG, "Attached driver to network interface");
}

void Netif::setNetifMac(wifi_interface_t wifi_interface) {
    uint8_t* mac = state::getThisMac().addr.data();
    esp_netif_set_mac(netif_.get(), mac);
    ESP_LOGI(TAG, "Set netif MAC to: " MACSTR, MAC2STR(mac));
}

using meshnow::lwip::netif::RootNetif;

esp_netif_t* RootNetif::createInterface() {
    ESP_LOGI(TAG, "Creating custom network interface for root node (AP)");
    // inherit from default config
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP();
    base_cfg.if_desc = "MeshNow Root";
    // set ip info because we want to establish a subnet
    base_cfg.ip_info = &subnet_ip;

    // create interface
    esp_netif_config_t cfg = {
        .base = &base_cfg,
        .driver = nullptr,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP,
    };
    esp_netif_t* netif = esp_netif_new(&cfg);
    if (netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create custom network interface for root node (AP)");
        ESP_ERROR_CHECK(ESP_FAIL);  // TODO
    }

    ESP_LOGI(TAG, "Custom network interface for root node (AP) created");
    return netif;
}

std::unique_ptr<meshnow::lwip::io::IODriverImpl> RootNetif::createIODriverImpl() {
    return std::make_unique<meshnow::lwip::io::RootIODriver>(netif_);
}

void RootNetif::start() {
    Netif::start();

    // set dhcp
    set_dhcp_dns();

    // set mac
    setNetifMac(WIFI_IF_AP);

    // start netif action
    ESP_LOGI(TAG, "Starting network interface for root node (AP)");
    esp_netif_action_start(netif_.get(), nullptr, 0, nullptr);
    ESP_LOGI(TAG, "Started network interface for root node (AP)");

    ip_napt_enable(subnet_ip.ip.addr, 1);
}

void RootNetif::stop() {
    ESP_LOGI(TAG, "Stopping network interface for root node (AP)");
    esp_netif_action_stop(netif_.get(), nullptr, 0, nullptr);
    ESP_LOGI(TAG, "Stopped network interface for root node (AP)");

    ip_napt_enable(subnet_ip.ip.addr, 0);
}

void RootNetif::set_dhcp_dns() {
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = DNS_IP_ADDR;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;

    ESP_LOGI(TAG, "Setting DHCP DNS to: %s", ip4addr_ntoa(reinterpret_cast<ip4_addr_t*>(&dns.ip.u_addr.ip4)));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif_.get(), ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &dhcps_dns_value, sizeof(dhcps_dns_value)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif_.get(), ESP_NETIF_DNS_MAIN, &dns));
    ESP_LOGI(TAG, "DHCP DNS set");
}

using meshnow::lwip::netif::NodeNetif;

esp_netif_t* NodeNetif::createInterface() {
    ESP_LOGI(TAG, "Creating custom network interface for node (STA)");
    // inherit from default config
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base_cfg.if_desc = "MeshNow Node";

    // create interface
    esp_netif_config_t cfg = {
        .base = &base_cfg,
        .driver = nullptr,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    esp_netif_t* netif = esp_netif_new(&cfg);
    if (netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create custom network interface for node (STA)");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    ESP_LOGI(TAG, "Custom network interface for node (STA) created");
    return netif;
}

std::unique_ptr<meshnow::lwip::io::IODriverImpl> NodeNetif::createIODriverImpl() {
    return std::make_unique<meshnow::lwip::io::NodeIODriver>(netif_);
}

void NodeNetif::start() {
    Netif::start();

    // set mac
    setNetifMac(WIFI_IF_STA);

    // start netif action
    ESP_LOGI(TAG, "Starting network interface for node (STA)");
    esp_netif_action_start(netif_.get(), nullptr, 0, nullptr);
    esp_netif_action_connected(netif_.get(), nullptr, 0, nullptr);
    ESP_LOGI(TAG, "Started network interface for node (STA)");
}

void NodeNetif::stop() {
    ESP_LOGI(TAG, "Stopping network interface for node (STA)");
    esp_netif_action_disconnected(netif_.get(), nullptr, 0, nullptr);
    esp_netif_action_stop(netif_.get(), nullptr, 0, nullptr);
    ESP_LOGI(TAG, "Stopped network interface for node (STA)");
}

}  // namespace meshnow::lwip::netif