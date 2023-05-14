#include "meshnow.hpp"

#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <mutex>

#include "error.hpp"
#include "internal.hpp"

static const char *TAG = CREATE_TAG("🦌");

static std::mutex mtx;

namespace meshnow {

/**
 * Initializes NVS.
 */
void App::initNVS() {
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
void App::initWifi() {
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
void App::deinitWifi() {
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
void App::initEspnow() {
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
void App::deinitEspnow() {
    ESP_LOGI(TAG, "Deinitializing ESP-NOW");

    CHECK_THROW(esp_now_unregister_recv_cb());
    CHECK_THROW(esp_now_unregister_send_cb());
    CHECK_THROW(esp_now_deinit());

    networking_callback_ptr = nullptr;

    ESP_LOGI(TAG, "ESP-NOW deinitialized");
}

App::App(const Config config) : config_{config}, state_{State::STOPPED}, networking_{state_} {
    std::scoped_lock lock{mtx};

    ESP_LOGI(TAG, "Initializing MeshNOW");
    initNVS();
    initWifi();
    initEspnow();
    ESP_LOGI(TAG, "MeshNOW initialized. You can start the mesh now 🦌");
}

App::~App() {
    if (state_ != State::STOPPED) {
        ESP_LOGW(TAG, "The mesh is still running. Stopping it for you. Consider calling stop() yourself! >:(");
        stop();
    }

    std::scoped_lock lock{mtx};

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

void App::start() {
    std::scoped_lock lock{mtx};

    if (state_ != State::STOPPED) {
        ESP_LOGE(TAG, "MeshNOW is already running");
        throw AlreadyStartedException();
    }
    state_ = State::STARTED;

    if (config_.root) {
        ESP_LOGI(TAG, "Starting MeshNOW as root...");
        // TODO start root
    } else {
        ESP_LOGI(TAG, "Starting MeshNOW as node...");
        // TODO start node
    }

    ESP_LOGI(TAG, "Liftoff! 🚀");
}

void App::stop() {
    std::scoped_lock lock{mtx};

    if (state_ != State::STARTED) {
        ESP_LOGE(TAG, "MeshNOW is not running");
        throw NotStartedException();
    }
    state_ = State::STOPPED;

    if (config_.root) {
        ESP_LOGI(TAG, "Stopping MeshNOW as root...");
        // TODO stop root
    } else {
        ESP_LOGI(TAG, "Stopping MeshNOW as node...");
        // TODO stop node
    }

    ESP_LOGI(TAG, "Mesh stopped! 🛑");
}

}  // namespace meshnow
