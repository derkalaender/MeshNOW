
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <array>

#include "meshnow.h"

static const char *TAG = "main";

static const std::array<uint8_t, 6> root{0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

extern "C" void app_main(void) {
    // INIT //
    {
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // init netif
        ESP_ERROR_CHECK(esp_netif_init());
    }
    // INIT DONE //

    std::array<uint8_t, 6> my_mac{};
    esp_read_mac(my_mac.data(), ESP_MAC_WIFI_STA);

    wifi_sta_config_t sta_config{.ssid = "marvin-hotspot", .password = "atleast8characters"};

    bool is_root = my_mac == root;

    meshnow_config_t config{.root = is_root,
                            .router_config = {
                                .should_connect = true,
                                .sta_config = &sta_config,
                            }};
    meshnow_init(&config);

    meshnow_start();
}
