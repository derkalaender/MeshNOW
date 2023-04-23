#include "meshnow.h"

#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "meshnow_internal.h"

const char *TAG = "✨MeshNOW✨";

// TODO c++ exception handling

/**
 * Initializes NVS.
 */
static void nvs_init() {
    ESP_LOGI(TAG, "Initializing NVS");

    // initialize nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "NVS initialized");
}

/**
 * Initializes WiFi.
 */
static void wifi_init() {
    ESP_LOGI(TAG, "Initializing WiFi");

    // initialize the tcp stack (not needed atm)
    //    ESP_ERROR_CHECK(esp_netif_init());
    // create default event loop (not needed atm)
    //    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // TODO set ap/sta mode for root
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // no powersaving
    // TODO maybe turn this on later?
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // use long range mode -> up to 1km according to espressif
    // TODO this also needs to be different for root (LR + BGN)
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));

    ESP_LOGI(TAG, "WiFi initialized");
}

/**
 * Deinitializes WiFi.
 */
static void wifi_deinit() {
    ESP_LOGI(TAG, "Deinitializing WiFi");

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());

    ESP_LOGI(TAG, "WiFi deinitialized");
}

/**
 * Initializes ESP-NOW.
 */
static void espnow_init() {
    ESP_LOGI(TAG, "Initializing ESP-NOW");

    ESP_ERROR_CHECK(esp_now_init());

    ESP_LOGI(TAG, "ESP-NOW initialized");
}

/**
 * Deinitializes ESP-NOW.
 */
static void espnow_deinit() {
    ESP_LOGI(TAG, "Deinitializing ESP-NOW");

    ESP_ERROR_CHECK(esp_now_unregister_recv_cb());
    ESP_ERROR_CHECK(esp_now_unregister_send_cb());
    ESP_ERROR_CHECK(esp_now_deinit());

    ESP_LOGI(TAG, "ESP-NOW deinitialized");
}

MeshNOW::MeshNOW() {
    ESP_LOGI(TAG, "Initializing MeshNOW");
    nvs_init();
    wifi_init();
    espnow_init();
    ESP_LOGI(TAG, "MeshNOW initialized. You can start the mesh now :D");
}

MeshNOW::~MeshNOW() {
    ESP_LOGI(TAG, "Deinitializing MeshNOW");
    espnow_deinit();
    wifi_deinit();
    // TODO deinit nvs?
    ESP_LOGI(TAG, "MeshNOW deinitialized. Goodbye!");
}
