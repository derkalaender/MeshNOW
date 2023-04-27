#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <memory>

#include "espnow_test.h"
#include "meshnow.hpp"
#include "networking.hpp"

static const char *TAG = "main";

void perform_range_test() {
    espnow_test_init();

    // setup button on gpio27 using internal pullup
    gpio_set_direction(GPIO_NUM_27, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_27, GPIO_PULLUP_ONLY);

    while (1) {
        // wait for button press
        while (gpio_get_level(GPIO_NUM_27) == 1) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        // button pressed, perform range test
        ESP_LOGI(TAG, "Starting range test");

        espnow_test_perform(100);
    }
}

static std::unique_ptr<MeshNOW::App> meshnow;

extern "C" void app_main(void) {
    meshnow = std::make_unique<MeshNOW::App>(MeshNOW::Config{.root = true});
    meshnow->start();

    auto target = MeshNOW::BROADCAST_MAC_ADDR;
    std::string s{"Creative test message"};
    std::vector<uint8_t> data{s.begin(), s.end()};
    auto buffer = MeshNOW::Packet::DataLwIP(target, 30, true, 1500, data).serialize();
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer.data(), buffer.size(), ESP_LOG_INFO);
}