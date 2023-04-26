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

/**
 * Initializes NVS.
 */
static void nvs_init() {
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
static void wifi_init() {
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
static void wifi_deinit() {
    ESP_LOGI(TAG, "Deinitializing WiFi");

    CHECK_THROW(esp_wifi_stop());
    CHECK_THROW(esp_wifi_deinit());

    ESP_LOGI(TAG, "WiFi deinitialized");
}

/**
 * Initializes ESP-NOW.
 */
static void espnow_init() {
    ESP_LOGI(TAG, "Initializing ESP-NOW");

    CHECK_THROW(esp_now_init());

    ESP_LOGI(TAG, "ESP-NOW initialized");
}

/**
 * Deinitializes ESP-NOW.
 */
static void espnow_deinit() {
    ESP_LOGI(TAG, "Deinitializing ESP-NOW");

    CHECK_THROW(esp_now_unregister_recv_cb());
    CHECK_THROW(esp_now_unregister_send_cb());
    CHECK_THROW(esp_now_deinit());

    ESP_LOGI(TAG, "ESP-NOW deinitialized");
}

using namespace MeshNOW;

App::App(const Config config) : config{config} {
    std::scoped_lock lock{mtx};

    ESP_LOGI(TAG, "Initializing MeshNOW");
    nvs_init();
    wifi_init();
    espnow_init();
    ESP_LOGI(TAG, "MeshNOW initialized. You can start the mesh now 🦌");
}

App::~App() {
    std::scoped_lock lock{mtx};

    ESP_LOGI(TAG, "Deinitializing MeshNOW");
    try {
        espnow_deinit();
        wifi_deinit();
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

    if (state != State::STOPPED) {
        ESP_LOGE(TAG, "MeshNOW is already running");
        throw AlreadyStartedException();
    }
    state = State::STARTED;

    if (config.root) {
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

    if (state != State::STARTED) {
        ESP_LOGE(TAG, "MeshNOW is not running");
        throw NotStartedException();
    }
    state = State::STOPPED;

    if (config.root) {
        ESP_LOGI(TAG, "Stopping MeshNOW as root...");
        // TODO stop root
    } else {
        ESP_LOGI(TAG, "Stopping MeshNOW as node...");
        // TODO stop node
    }

    ESP_LOGI(TAG, "Mesh stopped! 🛑");
}
