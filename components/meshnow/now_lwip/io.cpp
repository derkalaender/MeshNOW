#include "now_lwip/io.hpp"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>

#include "constants.hpp"
#include "internal.hpp"

const char* TAG = CREATE_TAG("LWIP | IO");

// STATIC IMPLS FOR DRIVER CONFIG

static esp_err_t transmit_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len) {
    return static_cast<meshnow::lwip::io::IODriver*>(driver_handle)->driver_impl->transmit(buffer, len);
}

static esp_err_t transmit_wrap_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len,
                                      void* netstack_buffer) {
    // we simply call the non-wrap version. the normal wifi driver does this too. see <wifi_netif.c>/wifi_transmit_wrap
    return transmit_static(driver_handle, buffer, len);
}

static void driver_free_rx_buffer_static(esp_netif_iodriver_handle driver_handle, void* buffer) {
    // simply free
    if (buffer) {
        free(buffer);
    }
}

esp_err_t meshnow::lwip::io::postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle) {
    ESP_LOGI(TAG, "Post attach callback called");
    auto driver = static_cast<meshnow::lwip::io::IODriver*>(driver_handle);
    driver->base.netif = esp_netif;
    esp_netif_driver_ifconfig_t driver_ifconfig = {.handle = driver,
                                                   .transmit = transmit_static,
                                                   .transmit_wrap = transmit_wrap_static,
                                                   .driver_free_rx_buffer = driver_free_rx_buffer_static};
    return esp_netif_set_driver_config(esp_netif, &driver_ifconfig);
}

using meshnow::lwip::io::IODriverImpl;

IODriverImpl::IODriverImpl(std::shared_ptr<esp_netif_t> netif, std::shared_ptr<SendWorker> send_worker,
                           std::shared_ptr<routing::Layout> layout)
    : layout_(std::move(layout)), send_worker_(std::move(send_worker)), netif_(std::move(netif)) {}

void meshnow::lwip::io::IODriverImpl::receivedData(const meshnow::Buffer& buffer) {
    ESP_LOGI(TAG, "Received data via MeshNOW. Forwarding to Netif layer");
    void* data_ptr = const_cast<void*>(reinterpret_cast<const void*>(buffer.data()));
    esp_netif_receive(netif_.get(), data_ptr, buffer.size(), nullptr);
}

void IODriverImpl::sendData(const meshnow::MAC_ADDR& mac, void* buffer, size_t len) {
    ESP_LOGI(TAG, "Sending data of length %d", len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_INFO);

    // TODO split and send
}

using meshnow::lwip::io::RootIODriver;

esp_err_t RootIODriver::transmit(void* buffer, size_t len) {
    meshnow::MAC_ADDR dest_mac;
    std::copy(static_cast<uint8_t*>(buffer), static_cast<uint8_t*>(buffer) + 6, dest_mac.begin());

    ESP_LOGI(TAG, "Transmitting packet to node: " MAC_FORMAT, MAC_FORMAT_ARGS(dest_mac));

    if (dest_mac == meshnow::BROADCAST_MAC_ADDR) {
        ESP_LOGI(TAG, "Broadcast not yet implemented");
        // TODO
        return ESP_OK;
    }

    // handle normal mac case
    sendData(dest_mac, buffer, len);

    return ESP_OK;
}

using meshnow::lwip::io::NodeIODriver;

esp_err_t NodeIODriver::transmit(void* buffer, size_t len) {
    meshnow::MAC_ADDR dest_mac;
    std::copy(static_cast<uint8_t*>(buffer), static_cast<uint8_t*>(buffer) + 6, dest_mac.begin());

    ESP_LOGI(TAG, "Transmitting packet to root, with dest addr: " MAC_FORMAT, MAC_FORMAT_ARGS(dest_mac));

    // TODO maybe shortcut if the dest mac is a child or the parent, we can directly send to those without going through
    // the root

    assert(layout_->root);
    sendData(*layout_->root, buffer, len);

    return ESP_OK;
}