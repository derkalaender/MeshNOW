#include "meshnow.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "custom.hpp"
#include "event.hpp"
#include "networking.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"
#include "wifi.hpp"

static constexpr auto* TAG = CREATE_TAG("🦌");

ESP_EVENT_DEFINE_BASE(MESHNOW_EVENT);

static bool initialized = false;
static bool started = false;

static meshnow::Networking networking;

static bool checkNVS() {
    // check if NVS is initialized
    // do dummy operation
    nvs_stats_t stats;
    if (nvs_get_stats(nullptr, &stats) == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGE(TAG, "NVS is not initialized!");
        return false;
    } else {
        ESP_LOGI(TAG, "NVS OK!");
    }
    return true;
}

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

static bool checkNetif() {
    ESP_LOGW(TAG,
             "Cannot check if Netif is initialized due to technical limitations.\n"
             "Please make sure to have called esp_netif_init() exactly once before initializing MeshNOW.\n"
             "Otherwise, the device might crash due to Netif/LWIP errors.");

    return true;
}

extern "C" esp_err_t meshnow_init(meshnow_config_t* config) {
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

    ESP_LOGI(TAG, "Checking required ESP-IDF components (NVS, Wi-Fi, Netif) are properly initialized...");
    if (checkNVS() && checkWiFi() && checkNetif()) {
        ESP_LOGI(TAG, "Check OK!");
    } else {
        ESP_LOGE(TAG, "Check failed!");
        return ESP_ERR_INVALID_STATE;
    }

    // create meshnow namespace
    {
        nvs_handle_t nvs_handle;
        ESP_RETURN_ON_ERROR(nvs_open("meshnow", NVS_READWRITE, &nvs_handle), TAG, "Creating NVS namespace failed");
        nvs_close(nvs_handle);
    }

    // init internal event loop
    ESP_RETURN_ON_ERROR(meshnow::event::Internal::init(), TAG, "Initializing internal event loop failed");

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

    ESP_RETURN_ON_ERROR(meshnow::wifi::init(), TAG, "Initializing Wi-Fi failed");

    // init custom cb collection
    meshnow::custom::init();

    // init networking
    ESP_RETURN_ON_ERROR(networking.init(), TAG, "Initializing networking failed");

    initialized = true;

    ESP_LOGI(TAG, "MeshNOW initialized. You can start the mesh now 🦌");

    return ESP_OK;
}

extern "C" esp_err_t meshnow_deinit() {
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
    meshnow::event::Internal::deinit();
    meshnow::custom::deinit();

    ESP_RETURN_ON_ERROR(meshnow::wifi::deinit(), TAG, "Deinitializing Wi-Fi failed");

    initialized = false;

    ESP_LOGI(TAG, "MeshNOW deinitialized. Goodbye 👋");
    return ESP_OK;
}

extern "C" esp_err_t meshnow_start() {
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

extern "C" esp_err_t meshnow_stop() {
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

extern "C" esp_err_t meshnow_send(uint8_t* dest, uint8_t* buffer, size_t len) {
    if (!initialized) {
        ESP_LOGE(TAG, "MeshNOW is not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    if (!started) {
        ESP_LOGE(TAG, "MeshNOW is not started!");
        return ESP_ERR_INVALID_STATE;
    }

    if (len > MESHNOW_MAX_CUSTOM_MESSAGE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO only send when have upstream connection...

    auto cxxBuffer = std::vector<uint8_t>(buffer, buffer + len);
    auto packet = meshnow::packets::CustomData{std::move(cxxBuffer)};
    auto resolve = meshnow::send::FullyResolve{
        meshnow::state::getThisMac(),
        meshnow::util::MacAddr{dest},
        meshnow::state::getThisMac(),
    };
    meshnow::send::enqueuePayload(std::move(packet), std::move(resolve));
}

extern "C" esp_err_t meshnow_register_data_cb(meshnow_data_cb_t cb, meshnow_data_cb_handle_t* handle) {
    // TODO error checking
    auto internal_handle = meshnow::custom::createCBHandle(cb);
    *handle = internal_handle;
}

extern "C" esp_err_t meshnow_unregister_data_cb(meshnow_data_cb_handle_t handle) {
    // TODO error checking
    meshnow::custom::destroyCBHandle(static_cast<meshnow::custom::ActualCBHandle*>(handle));
}