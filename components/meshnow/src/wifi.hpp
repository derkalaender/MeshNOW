#pragma once

#include <esp_err.h>
#include <esp_wifi_types.h>

namespace meshnow::wifi {

/**
 * Sets the Wi-Fi station configuration.
 */
void setConfig(wifi_sta_config_t* sta_config);

/**
 * Set if the root should connect to a router.
 */
void setShouldConnect(bool should_connect);

/**
 * Starts Wi-Fi and sets up netif.
 */
esp_err_t start();

/**
 * Stops Wi-Fi.
 */
esp_err_t stop();

}  // namespace meshnow::wifi