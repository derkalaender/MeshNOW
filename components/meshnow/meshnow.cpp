#include "meshnow.hpp"

#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "error.hpp"
#include "internal.hpp"

static const char *TAG = CREATE_TAG("🦌");

/**
 * Initializes NVS.
 */
void meshnow::Mesh::initNVS() {
    ESP_LOGI(TAG, "Initializing NVS");

    // initialize nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        CHECK_THROW(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    CHECK_THROW(ret);

    ESP_LOGI(TAG, "NVS initialized");
}

/**
 * Initializes WiFi.
 */
void meshnow::Mesh::initWifi() {
    ESP_LOGI(TAG, "Initializing WiFi");

    // initialize the tcp stack (not needed atm)
    //    ESP_ERROR_CHECK(esp_netif_init());

    // create default event loop (wifi posts events and will otherwise spam the logs)
    CHECK_THROW(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    CHECK_THROW(esp_wifi_init(&cfg));
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

    ESP_LOGI(TAG, "WiFi initialized");
}

/**
 * Deinitializes WiFi.
 */
void meshnow::Mesh::deinitWifi() {
    ESP_LOGI(TAG, "Deinitializing WiFi");

    CHECK_THROW(esp_wifi_stop());
    CHECK_THROW(esp_wifi_deinit());

    ESP_LOGI(TAG, "WiFi deinitialized");
}

// this is a dirty hack because esp-now callbacks are C-style...
static meshnow::Networking *networking_callback_ptr;

/**
 * Initializes ESP-NOW.
 */
void meshnow::Mesh::initEspnow() {
    ESP_LOGI(TAG, "Initializing ESP-NOW");

    CHECK_THROW(esp_now_init());
    networking_callback_ptr = &networking_;
    CHECK_THROW(esp_now_register_send_cb(
        [](auto mac_addr, auto status) { networking_callback_ptr->onSend(mac_addr, status); }));
    CHECK_THROW(esp_now_register_recv_cb(
        [](auto info, auto data, auto len) { networking_callback_ptr->onReceive(info, data, len); }));

    ESP_LOGI(TAG, "ESP-NOW initialized");
}

/**
 * Deinitializes ESP-NOW.
 */
void meshnow::Mesh::deinitEspnow() {
    ESP_LOGI(TAG, "Deinitializing ESP-NOW");

    CHECK_THROW(esp_now_unregister_recv_cb());
    CHECK_THROW(esp_now_unregister_send_cb());
    CHECK_THROW(esp_now_deinit());

    networking_callback_ptr = nullptr;

    ESP_LOGI(TAG, "ESP-NOW deinitialized");
}

meshnow::Mesh::Mesh(const Config config)
    : config_{config}, state_{std::make_shared<NodeState>(config_.root)}, networking_{state_} {
    ESP_LOGI(TAG, "Initializing MeshNOW");
    initNVS();
    initWifi();
    initEspnow();
    ESP_LOGI(TAG, "MeshNOW initialized. You can started the mesh now 🦌");
}

meshnow::Mesh::~Mesh() {
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

    ESP_LOGI(TAG, "Deinitializing MeshNOW");

    try {
        deinitEspnow();
        deinitWifi();
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Error while deinitializing MeshNOW: %s", e.what());
        ESP_LOGE(TAG, "This should never happen. Terminating....");
        std::terminate();
    }

    // TODO deinit nvs?
    ESP_LOGI(TAG, "MeshNOW deinitialized. Goodbye 👋");
}

void meshnow::Mesh::start() {
    auto lock = state_.acquireLock();

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
