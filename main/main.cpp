#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <memory>

// #include "espnow_test.h"
#include "meshnow.hpp"
#include "networking.hpp"

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

static std::unique_ptr<meshnow::App> MeshNOW;

static const meshnow::MAC_ADDR root{0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

extern "C" void app_main(void) {
    meshnow::MAC_ADDR my_mac;
    esp_read_mac(my_mac.data(), ESP_MAC_WIFI_STA);

    bool is_root = my_mac == root;

    MeshNOW = std::make_unique<meshnow::App>(meshnow::Config{.root = is_root});
    ESP_LOGI(TAG, "MeshNOW initialized");
    ESP_LOGI(TAG, "Starting as %s!", is_root ? "root" : "node");
    MeshNOW->start();

    auto target = meshnow::BROADCAST_MAC_ADDR;
    std::string s{"Creative test message"};
    std::vector<uint8_t> data{s.begin(), s.end()};
    auto payload = meshnow::packets::DataFirstPayload(target, 30, 1500, false, data);
    auto buffer = meshnow::packets::Packet(payload).serialize();
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer.data(), buffer.size(), ESP_LOG_INFO);
}
