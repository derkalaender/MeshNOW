
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_now_multi.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <memory>
#include <meshnow.hpp>
#include <thread>

#include "mqtt_demo.hpp"

static const char *TAG = "main";

static constexpr auto IP_BIT = BIT0;

// void perform_range_test() {
//     espnow_test_init();
//
//     // setup button on gpio27 using internal pullup
//     gpio_set_direction(GPIO_NUM_27, GPIO_MODE_INPUT);
//     gpio_set_pull_mode(GPIO_NUM_27, GPIO_PULLUP_ONLY);
//
//     while (1) {
//         // wait for button press
//         while (gpio_get_level(GPIO_NUM_27) == 1) {
//             vTaskDelay(10 / portTICK_PERIOD_MS);
//         }
//
//         // button pressed, perform range test
//         ESP_LOGI(TAG, "Starting range test");
//
//         espnow_test_perform(100);
//     }
// }

static util::WaitBits wait_bits;

static const meshnow::MAC_ADDR root{0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

static std::unique_ptr<meshnow::Mesh> MeshNOW;

static esp_now_multi_handle_t multi_handle;

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == IP_EVENT);

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            auto *event = static_cast<ip_event_got_ip_t *>(event_data);
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            wait_bits.setBits(IP_BIT);
            break;
        }
        case IP_EVENT_STA_LOST_IP: {
            ESP_LOGI(TAG, "Lost IP");
            break;
        }
        case IP_EVENT_AP_STAIPASSIGNED: {
            auto *event = static_cast<ip_event_ap_staipassigned_t *>(event_data);
            ESP_LOGI(TAG, "Assigned IP: " IPSTR " to " MAC_FORMAT, IP2STR(&event->ip), MAC_FORMAT_ARGS(event->mac));
        }
        default:
            break;
    }
}

void mqtt_thread(void *arg) {
    // wait until we got IP
    auto bits = wait_bits.waitFor(IP_BIT, true, true, portMAX_DELAY);
    assert(bits == IP_BIT);

    ESP_LOGI(TAG, "Starting MQTT Demo");
    MQTTDemo mqtt_demo;
    mqtt_demo.run();
}

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

        ESP_ERROR_CHECK(esp_now_init());
        ESP_ERROR_CHECK(esp_now_multi_init());

        // init netif
        ESP_ERROR_CHECK(esp_netif_init());
    }
    // INIT DONE //

    meshnow::MAC_ADDR my_mac;
    esp_read_mac(my_mac.data(), ESP_MAC_WIFI_STA);

    wifi_sta_config_t sta_config{.ssid = "marvin-hotspot", .password = "atleast8characters"};

    bool is_root = my_mac == root;

    ESP_LOGI(TAG, "Initializing MeshNOW");
    meshnow::Config config{.root = is_root, .sta_config = sta_config};
    MeshNOW = std::make_unique<meshnow::Mesh>(config);
    ESP_LOGI(TAG, "MeshNOW initialized");

    ESP_LOGI(TAG, "Setting up Multi ESP-NOW");

    meshnow::Callbacks callbacks{MeshNOW->getCallbacks()};
    esp_now_multi_reg_t reg{callbacks.recv_cb, callbacks.send_cb, callbacks.arg};
    esp_now_multi_register(reg, &multi_handle);
    ESP_LOGI(TAG, "Multi ESP-NOW set up");

    ESP_LOGI(TAG, "Registering IP event handler");
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, nullptr));

    ESP_LOGI(TAG, "Starting as %s!", is_root ? "root" : "node");

    MeshNOW->start();

    //    if (!config.root) {
    ESP_LOGI(TAG, "Starting MQTT thread");
    std::thread mqtt(mqtt_thread, nullptr);
    mqtt.detach();
    //    }
}
