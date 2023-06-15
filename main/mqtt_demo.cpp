#include "mqtt_demo.hpp"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>

#include "waitbits.hpp"

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
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);

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
            ESP_LOGI(TAG, "Broker acknowledged publish, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "Error in MQTT");
            break;
        default:
            ESP_LOGI(TAG, "Other event id: %ld", event_id);
            break;
    }
}

static std::string macToStr(std::array<uint8_t, 6> mac) {
    std::string result;
    for (auto i = 0; i < 6; i++) {
        result += std::to_string(mac[i]);
        if (i != 5) {
            result += ":";
        }
    }
    return result;
}

void MQTTDemo::run() {
    wait_bits.waitFor(CONNECT_BIT, true, true, portMAX_DELAY);

    std::array<uint8_t, 6> mac = {0};
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);

    auto topic = "random/hgasvdhgauztdzuasgdjhbahm";
    auto message = "[" + macToStr(mac) + "]: hello world";

    ESP_LOGI(TAG, "Publishing to topic %s: %s", topic, message.c_str());
    while (true) {
        publish(topic, message.c_str());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void MQTTDemo::publish(const char *topic, const char *message) {
    if (esp_mqtt_client_publish(client.get(), topic, message, 0, 1, 0)) {
        ESP_LOGI(TAG, "Published message");
    } else {
        ESP_LOGE(TAG, "Failed to publish message");
    }
}
