#include "mqtt_demo.hpp"

#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <map>

#include "bh1750_handler.h"
#include "constants.hpp"
#include "dht11.h"

static const char *TAG = "mqtt_demo";

static constexpr auto CONNECT_BIT = BIT0;

static void mqtt_event_handler_wrap(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    static_cast<MQTTDemo *>(handler_args)->mqtt_event_handler(handler_args, base, event_id, event_data);
}

MQTTDemo::MQTTDemo() {
    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;

    auto client_ptr = esp_mqtt_client_init(&mqtt_cfg);
    assert(client_ptr);
    client.reset(client_ptr);

    esp_mqtt_client_register_event(client.get(), static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                   &mqtt_event_handler_wrap, this);

    esp_mqtt_client_start(client.get());
}

void MQTTDemo::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGV(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);

    auto event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker with URI %s", MQTT_BROKER_URI);
            wait_bits.setBits(CONNECT_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from MQTT broker");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGV(TAG, "Broker acknowledged publish, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "Error in MQTT");
            break;
        default:
            ESP_LOGI(TAG, "Other event id: %ld", event_id);
            break;
    }
}

static meshnow::MAC_ADDR my_mac;

static constexpr auto topic = "random/hgasvdhgauztdzuasgdjhbahm";

void MQTTDemo::run() {
    wait_bits.waitFor(CONNECT_BIT, true, true, portMAX_DELAY);

    esp_read_mac(my_mac.data(), ESP_MAC_WIFI_STA);

    // send ready message
    publish(topic, "Ready");

    std::map<meshnow::MAC_ADDR, decltype(&MQTTDemo::run_root)> run_map{
        {{0x24, 0x6F, 0x28, 0x4A, 0x63, 0x3C}, &MQTTDemo::run_root},
        {{0xBC, 0xDD, 0xC2, 0xCC, 0xC3, 0x0C}, &MQTTDemo::run_temphum},
        {{0xBC, 0xDD, 0xC2, 0xCF, 0xFE, 0xAC}, &MQTTDemo::run_lux},
        {{0x24, 0x6F, 0x28, 0x4A, 0x66, 0xCC}, &MQTTDemo::run_camera},
    };

    // run specific function
    auto fnc = run_map[my_mac];
    (this->*fnc)();

    ESP_LOGI(TAG, "Finished!");
}

void MQTTDemo::publish(const char *topic, const char *message) {
    // asprintf "[mac as hex][message]"
    char *data;
    asprintf(&data, "[" MAC_FORMAT "][%s]", MAC_FORMAT_ARGS(my_mac), message);

    if (esp_mqtt_client_publish(client.get(), topic, data, 0, 0, 0) >= 0) {
        ESP_LOGD(TAG, "Published message");
    } else {
        ESP_LOGE(TAG, "Failed to publish message");
    }
    free(data);
}

void MQTTDemo::run_root() {
    ESP_LOGI(TAG, "Running as root");
    publish(topic, "I am (g)root!");
    ESP_LOGI(TAG, "Nothing more do do");
}

void MQTTDemo::run_temphum() {
    ESP_LOGI(TAG, "Running as temphum");

    ESP_LOGI(TAG, "Initializing DHT11");
    DHT11_init(GPIO_NUM_27);

    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true) {
        auto value = DHT11_read();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Temp: %d, Hum: %d", value.temperature, value.humidity);
        char *text;
        asprintf(&text, "Temperature: %d°C | Humidity: %d%%", value.temperature, value.humidity);
        publish(topic, text);
        free(text);
    }

    ESP_LOGI(TAG, "Starting measure");
}

void MQTTDemo::run_lux() {
    ESP_LOGI(TAG, "Running as lux");

    ESP_LOGI(TAG, "Initializing BH1750");
    caps_bh1750_i2c_init();
    caps_bh1750_init();

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Starting measure");

    while (true) {
        uint16_t value = caps_bh1750_measure();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Lux: %d", value);
        char *text;
        asprintf(&text, "Luminance: %dlx", value);
        publish(topic, text);
        free(text);
    }
}

void MQTTDemo::run_camera() {
    ESP_LOGI(TAG, "Running as camera");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // TODO camera

    while (true) {
        publish(topic, "Taking fabulous pictures...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
