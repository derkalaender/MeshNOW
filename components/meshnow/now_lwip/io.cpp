#include "now_lwip/io.hpp"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>

#include "internal.hpp"

const char* TAG = CREATE_TAG("LWIP | IO");

using meshnow::lwip::io::IODriver;

esp_err_t IODriver::postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle) {
    ESP_LOGI(TAG, "Post attach callback called");
    auto driver = static_cast<IODriver*>(driver_handle);
    driver->base.netif = esp_netif;
    esp_netif_driver_ifconfig_t driver_ifconfig = {.handle = driver,
                                                   .transmit = transmit_static,
                                                   .transmit_wrap = transmit_wrap_static,
                                                   .driver_free_rx_buffer = driver_free_rx_buffer_static};
    return esp_netif_set_driver_config(esp_netif, &driver_ifconfig);
}

esp_err_t IODriver::transmit_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len) {
    return static_cast<IODriver*>(driver_handle)->transmit(buffer, len);
}

esp_err_t IODriver::transmit_wrap_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len,
                                         void* netstack_buffer) {
    // we simply call the non-wrap version. the normal wifi driver does this too. see <wifi_netif.c>/wifi_transmit_wrap
    return static_cast<IODriver*>(driver_handle)->transmit_wrap(buffer, len, netstack_buffer);
}

void IODriver::driver_free_rx_buffer_static(esp_netif_iodriver_handle driver_handle, void* buffer) {
    static_cast<IODriver*>(driver_handle)->driver_free_rx_buffer(buffer);
}

esp_err_t IODriver::transmit(void* buffer, size_t len) { return 0; }

esp_err_t IODriver::transmit_wrap(void* buffer, size_t len, void* netstack_buffer) {
    // TODO what is netstack_buffer for?
    return transmit(buffer, len);
}

void IODriver::driver_free_rx_buffer(void* buffer) {
    // TODO maybe do something else besides freeing
    if (buffer) {
        free(buffer);
    }
}
