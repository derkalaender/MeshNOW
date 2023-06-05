
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_now_multi.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <memory>
#include <meshnow.hpp>

static const char *TAG = "main";

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

static const meshnow::MAC_ADDR root{0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

static std::unique_ptr<meshnow::Mesh> MeshNOW;

static esp_now_multi_handle_t multi_handle;

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
    }
    // INIT DONE //

    meshnow::MAC_ADDR my_mac;
    esp_read_mac(my_mac.data(), ESP_MAC_WIFI_STA);

    bool is_root = my_mac == root;

    ESP_LOGI(TAG, "Initializing MeshNOW");
    meshnow::Config config{.root = is_root};
    MeshNOW = std::make_unique<meshnow::Mesh>(config);
    ESP_LOGI(TAG, "MeshNOW initialized");

    ESP_LOGI(TAG, "Setting up Multi ESP-NOW");

    meshnow::Callbacks callbacks{MeshNOW->getCallbacks()};
    esp_now_multi_reg_t reg{callbacks.recv_cb, callbacks.send_cb, callbacks.arg};
    esp_now_multi_register(reg, &multi_handle);
    ESP_LOGI(TAG, "Multi ESP-NOW set up");

    ESP_LOGI(TAG, "Starting as %s!", is_root ? "root" : "node");

    MeshNOW->start();

    //    auto target = meshnow::BROADCAST_MAC_ADDR;
    //    std::string s{"Creative test message"};
    //    std::vector<uint8_t> data{s.begin(), s.end()};
    //    auto payload = meshnow::packets::DataFirstPayload(target, 30, 1500, false, data);
    //    auto buffer = meshnow::packets::Packet(payload).serialize();
    //    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer.data(), buffer.size(), ESP_LOG_INFO);
}
