#include "meshnow.hpp"

#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "error.hpp"
#include "internal.hpp"

static const char *TAG = CREATE_TAG("🦌");

static void checkWiFi() {
    // check if WiFi is initialized
    // do dummy operation
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "WiFi is not initialized");
        CHECK_THROW(ret);
    } else {
        ESP_LOGI(TAG, "WiFi OK!");
    }
}

static void checkEspNow() {
    // check if ESP-NOW is initialized
    // do dummy operation
    esp_now_peer_info_t peer;
    esp_err_t ret = esp_now_fetch_peer(true, &peer);
    if (ret == ESP_ERR_ESPNOW_NOT_INIT) {
        ESP_LOGE(TAG, "ESP-NOW is not initialized");
        CHECK_THROW(ret);
    } else {
        ESP_LOGI(TAG, "WiFi OK!");
        ESP_LOGI(TAG, "You may register the MeshNow callbacks now...");
    }
}

void setupWiFi() {
    ESP_LOGI(TAG, "Setting WiFi configuration");

    CHECK_THROW(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // TODO set ap/sta mode for root
    CHECK_THROW(esp_wifi_set_mode(WIFI_MODE_STA));
    CHECK_THROW(esp_wifi_start());

    // no powersaving
    // TODO maybe turn this on actually?
    CHECK_THROW(esp_wifi_set_ps(WIFI_PS_NONE));
    // use long range mode -> up to 1km according to espressif
    // TODO this also needs to be different for root (LR + BGN)
    CHECK_THROW(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));

    ESP_LOGI(TAG, "WiFi set up");
}

meshnow::Mesh::Mesh(const Config config)
    : config_{config}, state_{std::make_shared<NodeState>(config_.root)}, networking_{state_} {
    ESP_LOGI(TAG, "Initializing MeshNOW");
    ESP_LOGI(TAG, "Checking ESP-IDF network stack is properly initialized...");
    checkWiFi();
    checkEspNow();
    ESP_LOGI(TAG, "Check OK!");
    setupWiFi();
    ESP_LOGI(TAG, "MeshNOW initialized. You can started the mesh now 🦌");
}

meshnow::Mesh::~Mesh() {
    ESP_LOGI(TAG, "Deinitializing MeshNOW");

    if (state_->isStarted()) {
        ESP_LOGW(TAG, "The mesh is still running. Stopping it for you. Consider calling stop() yourself! >:(");
        try {
            stop();
        } catch (const std::exception &e) {
            ESP_LOGE(TAG, "Error while stopping MeshNOW: %s", e.what());
            ESP_LOGE(TAG, "This should never happen. Terminating....");
            std::terminate();
        }
    }

    ESP_LOGI(TAG, "MeshNOW deinitialized. Goodbye 👋");
}

void meshnow::Mesh::start() {
    auto lock = state_->acquireLock();

    if (state_->isStarted()) {
        ESP_LOGE(TAG, "MeshNOW is already running");
        throw AlreadyStartedException();
    }

    ESP_LOGI(TAG, "Starting MeshNOW as %s...", state_->isRoot() ? "root" : "node");

    networking_.start();

    ESP_LOGI(TAG, "Liftoff! 🚀");

    state_->setStarted(true);
}

void meshnow::Mesh::stop() {
    auto lock = state_->acquireLock();

    if (!state_->isStarted()) {
        ESP_LOGE(TAG, "MeshNOW is not running");
        throw NotStartedException();
    }

    ESP_LOGI(TAG, "Stopping MeshNOW as %s...", state_->isRoot() ? "root" : "node");

    networking_.stop();

    ESP_LOGI(TAG, "Mesh stopped! 🛑");

    state_->setStarted(false);
}
