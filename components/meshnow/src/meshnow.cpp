#include "meshnow.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "networking.hpp"
#include "state.hpp"
#include "util/util.hpp"
#include "wifi.hpp"

static constexpr auto *TAG = CREATE_TAG("🦌");

static bool initialized = false;
static bool started = false;

static meshnow::Networking networking;

static bool checkWiFi() {
    // check if WiFi is initialized
    // do dummy operation
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "WiFi is not initialized!");
        return false;
    } else {
        ESP_LOGI(TAG, "WiFi OK!");
    }
    return true;
}

static bool checkEspNow() {
    // check if ESP-NOW is initialized
    // do dummy operation
    esp_now_peer_info_t peer;
    if (esp_now_fetch_peer(true, &peer) == ESP_ERR_ESPNOW_NOT_INIT) {
        ESP_LOGE(TAG, "ESP-NOW is not initialized!");
        return false;
    } else {
        ESP_LOGI(TAG, "ESP-NOW OK!");
        ESP_LOGI(TAG, "You may register the MeshNow callbacks now...");
    }
    return true;
}

static bool checkNetif() {
    ESP_LOGW(TAG,
             "Cannot check if Netif is initialized due to technical limitations.\n"
             "Please make sure to have called esp_netif_init() exactly once before initializing MeshNOW.\n"
             "Otherwise, the device might crash due to Netif/LWIP errors.");

    return true;
}

esp_err_t meshnow_init(meshnow_config_t *config) {
    if (initialized) {
        ESP_LOGE(TAG, "MeshNOW is already initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    // check if config is valid
    if (config == nullptr) {
        ESP_LOGE(TAG, "Config is null!");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->root && config->router_config.should_connect && config->router_config.sta_config == nullptr) {
        ESP_LOGE(TAG, "Set this node as root and want to should_connect to a router but the STA config is null!");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing MeshNOW");

    ESP_LOGI(TAG, "Checking ESP-IDF network stack is properly initialized...");
    if (checkWiFi() && checkEspNow() && checkNetif()) {
        ESP_LOGI(TAG, "Check OK!");
    } else {
        ESP_LOGE(TAG, "Check failed!");
        return ESP_ERR_INVALID_STATE;
    }

    // init state
    ESP_RETURN_ON_ERROR(meshnow::state::init(), TAG, "Initializing state failed");

    // setup state
    meshnow::state::setRoot(config->root);

    // if root, we can always reach the root and know the root mac
    if (config->root) {
        meshnow::state::setRootMac(meshnow::state::getThisMac());
        meshnow::state::setState(meshnow::state::State::REACHES_ROOT);
    }

    // set router config if root
    if (config->root && config->router_config.should_connect) {
        // save Wi-Fi config
        meshnow::wifi::setConfig(config->router_config.sta_config);
        meshnow::wifi::setShouldConnect(true);
    } else {
        meshnow::wifi::setShouldConnect(false);
    }

    // init networking
    ESP_RETURN_ON_ERROR(networking.init(), TAG, "Initializing networking failed");

    initialized = true;

    ESP_LOGI(TAG, "MeshNOW initialized. You can start the mesh now 🦌");

    return ESP_OK;
}

esp_err_t meshnow_deinit() {
    if (!initialized) {
        ESP_LOGE(TAG, "MeshNOW is not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    if (started) {
        ESP_LOGW(TAG, "The mesh is still running. You should deinitialize it before calling stop!");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing MeshNOW");

    networking.deinit();
    meshnow::state::deinit()

        initialized = false;

    ESP_LOGI(TAG, "MeshNOW deinitialized. Goodbye 👋");
    return ESP_OK;
}

esp_err_t meshnow_start() {
    if (!initialized) {
        ESP_LOGE(TAG, "MeshNOW is not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    if (started) {
        ESP_LOGE(TAG, "MeshNOW is already started!");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting MeshNOW as '%s'", meshnow::state::isRoot() ? "root" : "node");

    // start Wi-Fi
    ESP_RETURN_ON_ERROR(meshnow::wifi::start(), TAG, "Starting Wi-Fi failed");

    // start networking
    ESP_RETURN_ON_ERROR(networking.start(), TAG, "Starting networking failed");

    started = true;

    ESP_LOGI(TAG, "Liftoff! 🚀");
    return ESP_OK;
}

esp_err_t meshnow_stop() {
    if (!initialized) {
        ESP_LOGE(TAG, "MeshNOW is not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    if (!started) {
        ESP_LOGE(TAG, "MeshNOW is not started!");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping MeshNOW");

    // stop networking
    networking.stop();

    // stop Wi-Fi
    ESP_RETURN_ON_ERROR(meshnow::wifi::stop(), TAG, "Stopping Wi-Fi failed");

    started = false;

    ESP_LOGI(TAG, "MeshNOW stopped! 🛑");
    return ESP_OK;
}