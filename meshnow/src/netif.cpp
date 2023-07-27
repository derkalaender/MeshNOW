#include "netif.hpp"

#include <dhcpserver/dhcpserver.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <lwip/ip4_addr.h>
#include <lwip/lwip_napt.h>

#include <memory>

#include "event.hpp"
#include "fragments.hpp"
#include "packets.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/mac.hpp"
#include "util/task.hpp"
#include "util/util.hpp"

namespace meshnow {

static const char* TAG = CREATE_TAG("Netif");

// use cloudflare 1.1.1.1 as DNS server
static constexpr auto DNS_IP_ADDR = ESP_IP4TOADDR(1, 1, 1, 1);

static const esp_netif_ip_info_t subnet_ip = {
    .ip = {.addr = ESP_IP4TOADDR(10, 0, 0, 1)},
    .netmask = {.addr = ESP_IP4TOADDR(255, 255, 0, 0)},
    .gw = {.addr = ESP_IP4TOADDR(10, 0, 0, 1)},
};

static util::Task io_receive_task_handle;

// forward declaration
/**
 * Post attach callback for netif. Will configure IO function callbacks.
 */
static esp_err_t postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle);

esp_err_t NowNetif::init() {
    netif_ptr netif = createInterface();
    if (!netif) {
        ESP_LOGE(TAG, "Failed to create network interface");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Network interface created");
    }
    netif_ = std::move(netif);

    // attach IO driver
    ESP_LOGI(TAG, "Attaching IO driver to network interface");
    // we only create the base driver since we don't need any special functionality
    io_driver_ = std::make_unique<esp_netif_driver_base_t>(&postAttachCallback, nullptr);
    ESP_RETURN_ON_ERROR(esp_netif_attach(netif_.get(), io_driver_.get()), TAG,
                        "Failed to attach IO driver to network interface");
    ESP_LOGI(TAG, "Attached IO driver to network interface");

    esp_err_t ret = ESP_OK;

    ret = setMac();
    if (ret != ESP_OK) {
        return ret;
    }

    if (state::isRoot()) {
        ret = initRootSpecific();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ret;
}

NowNetif::netif_ptr NowNetif::createInterface() {
    ESP_LOGI(TAG, "Creating custom network interface for %s", state::isRoot() ? "root (AP)" : "node (STA)");
    // inherit from default config
    esp_netif_inherent_config_t base_cfg;
    if (state::isRoot()) {
        base_cfg = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP();
        base_cfg.if_desc = "MeshNow Root";
        // set ip info because we want to establish a subnet
        base_cfg.ip_info = &subnet_ip;
    } else {
        base_cfg = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
        base_cfg.if_desc = "MeshNow Node";
    }

    esp_netif_config_t cfg = {
        .base = &base_cfg,
        .driver = nullptr,
        .stack = state::isRoot() ? ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP : ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };

    return netif_ptr{esp_netif_new(&cfg)};
}

esp_err_t NowNetif::setMac() {
    ESP_LOGI(TAG, "Setting MAC address");
    ESP_RETURN_ON_ERROR(esp_netif_set_mac(netif_.get(), state::getThisMac().addr.data()), TAG,
                        "Failed to set MAC address");
    ESP_LOGI(TAG, "MAC address set");
    return ESP_OK;
}

esp_err_t NowNetif::initRootSpecific() {
    // set dns & dhcp
    {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = DNS_IP_ADDR;
        dns.ip.type = ESP_IPADDR_TYPE_V4;

        ESP_LOGI(TAG, "Setting DHCP DNS to: %s", ip4addr_ntoa(reinterpret_cast<ip4_addr_t*>(&dns.ip.u_addr.ip4)));

        ESP_RETURN_ON_ERROR(esp_netif_set_dns_info(netif_.get(), ESP_NETIF_DNS_MAIN, &dns), TAG,
                            "Could not set DNS info");
        dhcps_offer_t dhcps_offer_dns = OFFER_DNS;
        ESP_RETURN_ON_ERROR(esp_netif_dhcps_option(netif_.get(), ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                                   &dhcps_offer_dns, sizeof(dhcps_offer_dns)),
                            TAG, "Could not update DHCP server");

        ESP_LOGI(TAG, "DHCP DNS set");
    }

    // enable network address port translation
    ip_napt_enable(subnet_ip.ip.addr, 1);

    return ESP_OK;
}

static void event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    assert(event_base == event::MESHNOW_INTERNAL && "Invalid event base");
    assert(event_id == static_cast<int32_t>(event::InternalEvent::STATE_CHANGED) && "Invalid event id");

    auto& netif = *static_cast<NowNetif*>(event_handler_arg);
    auto data = *static_cast<event::StateChangedEvent*>(event_data);

    if (data.old_state == state::State::DISCONNECTED_FROM_PARENT) {
        // connected to parent
        esp_netif_action_connected(netif.netif_.get(), nullptr, 0, nullptr);
    } else if (data.new_state == state::State::DISCONNECTED_FROM_PARENT) {
        // disconnected from parent
        esp_netif_action_disconnected(netif.netif_.get(), nullptr, 0, nullptr);
    }
}

void NowNetif::start() {
    ESP_LOGI(TAG, "Starting network interface");
    ESP_ERROR_CHECK(io_receive_task_handle.init(util::TaskSettings("io_receive", 2048, 4, util::CPU::PRO_CPU),
                                                [&] { io_receive_task(); }));
    esp_netif_action_start(netif_.get(), nullptr, 0, nullptr);
    event_handler_instance_.emplace(event::Internal::handle, event::MESHNOW_INTERNAL,
                                    static_cast<uint32_t>(event::InternalEvent::STATE_CHANGED), &event_handler, this);
    ESP_LOGI(TAG, "Started network interface");
}

void NowNetif::stop() {
    ESP_LOGI(TAG, "Stopping network interface");
    io_receive_task_handle = util::Task();
    event_handler_instance_.reset();
    esp_netif_action_stop(netif_.get(), nullptr, 0, nullptr);
    if (!state::isRoot()) {
        esp_netif_action_disconnected(netif_.get(), nullptr, 0, nullptr);
    }
    ESP_LOGI(TAG, "Stopped network interface");
}

void NowNetif::deinit() {
    if (state::isRoot()) {
        deinitRootSpecific();
    }

    io_driver_.reset();
    netif_.reset();
}

void NowNetif::deinitRootSpecific() { ip_napt_enable(subnet_ip.ip.addr, 0); }

//// TODO connect and disconnect
//

// IO DRIVER

[[noreturn]] void NowNetif::io_receive_task() {
    ESP_LOGI(TAG, "IO receive task started");

    while (true) {
        auto data = fragments::popReassembledData(portMAX_DELAY);
        if (!data) continue;

        esp_netif_receive(netif_.get(), data->data(), data->size(), nullptr);
        vTaskDelay(1000);
    }
}

/**
 * Fragment data into multiple fragments.
 * @param frag_id fragment id, will be used for all fragments
 * @param buffer pointer to the data to be sent, will be incremented
 * @param size_remaining size of the remaining data to be sent, will be decremented
 * @param frag_num current fragment number, will be incremented
 * @param total_size total size of the data to be sent
 * @return
 */
static meshnow::packets::DataFragment fragment(uint32_t frag_id, uint8_t*& buffer, size_t& size_remaining,
                                               uint8_t& frag_num, uint16_t total_size) {
    static constexpr auto MAX_FRAG_PAYLOAD_SIZE{250 - 8 - 19};

    util::Buffer data;

    if (size_remaining > MAX_FRAG_PAYLOAD_SIZE) {
        data = util::Buffer{buffer, buffer + MAX_FRAG_PAYLOAD_SIZE};
        size_remaining -= MAX_FRAG_PAYLOAD_SIZE;
        buffer += MAX_FRAG_PAYLOAD_SIZE;
    } else {
        data = util::Buffer{buffer, buffer + size_remaining};
        size_remaining = 0;
    }

    packets::DataFragment frag{
        .frag_id = frag_id,
        .options = {.unpacked =
                        {
                            .frag_num = frag_num,
                            .total_size = total_size,
                        }},
        .data = std::move(data),
    };

    frag_num++;

    return frag;
}

static esp_err_t transmit(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len) {
    assert(len > 0 && len <= 1500 && "Invalid length");

    util::MacAddr dest_mac{static_cast<uint8_t*>(buffer)};

    uint32_t frag_id = esp_random();

    auto* buffer8 = static_cast<uint8_t*>(buffer);
    size_t size_remaining = len;
    uint8_t frag_num = 0;

    while (size_remaining > 0) {
        auto frag = fragment(frag_id, buffer8, size_remaining, frag_num, len);

        if (state::isRoot()) {
            // transmit to the corresponding node
            send::enqueuePayload(std::move(frag),
                                 send::FullyResolve(state::getThisMac(), dest_mac, state::getThisMac()));
        } else {
            // transmit to the root
            send::enqueuePayload(std::move(frag),
                                 send::FullyResolve(state::getThisMac(), util::MacAddr::root(), state::getThisMac()));
        }
    }

    return ESP_OK;
}

static esp_err_t transmit_wrap(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len,
                               void* netstack_buffer) {
    // we simply call the non-wrap version. the normal wifi driver does this too. see <wifi_netif.c>/wifi_transmit_wrap
    return transmit(driver_handle, buffer, len);
}

static void driver_free_rx_buffer(esp_netif_iodriver_handle driver_handle, void* buffer) {
    // simply free
    if (buffer) {
        // TODO COULD BE A DOUBLE FREE
        free(buffer);
    }
}

static esp_err_t postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle) {
    ESP_LOGI(TAG, "Post attach callback called");

    // link netif to driver
    static_cast<esp_netif_driver_base_t*>(driver_handle)->netif = esp_netif;

    // create driver config
    esp_netif_driver_ifconfig_t driver_ifconfig = {.handle = driver_handle,
                                                   .transmit = transmit,
                                                   .transmit_wrap = transmit_wrap,
                                                   .driver_free_rx_buffer = driver_free_rx_buffer};
    return esp_netif_set_driver_config(esp_netif, &driver_ifconfig);
}

}  // namespace meshnow