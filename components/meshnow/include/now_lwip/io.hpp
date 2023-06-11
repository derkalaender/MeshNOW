#pragma once

#include <esp_err.h>
#include <esp_netif.h>

namespace meshnow::lwip::io {

class IODriver {
   private:
    /**
     * Post attach callback for netif. Will configure io function callbacks.
     */
    static esp_err_t postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle);

    // Static wrapper functions for the IO callbacks
    // For all of these, driver_handle is *this.
    // TODO make these noexcept

    static esp_err_t transmit_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len);
    static esp_err_t transmit_wrap_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len,
                                          void* netstack_buffer);
    static void driver_free_rx_buffer_static(esp_netif_iodriver_handle driver_handle, void* buffer);

    // Actual implementations
    esp_err_t transmit(void* buffer, size_t len);
    esp_err_t transmit_wrap(void* buffer, size_t len, void* netstack_buffer);
    void driver_free_rx_buffer(void* buffer);

    esp_netif_driver_base_t base{.post_attach = postAttachCallback, .netif = nullptr};
    // TODO other parameters
};

}  // namespace meshnow::lwip::io