#pragma once

#include <mqtt_client.h>

#include <memory>

class MQTTDemo {
   public:
    MQTTDemo();

    MQTTDemo(const MQTTDemo&) = delete;

    MQTTDemo& operator=(const MQTTDemo&) = delete;

    void run();

    void publish(const char* topic, const char* message);

    void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

   private:
    static constexpr auto MQTT_BROKER_URI = "mqtt://test.mosquitto.org";

    std::unique_ptr<struct esp_mqtt_client, decltype(&esp_mqtt_client_destroy)> client{nullptr, esp_mqtt_client_destroy};
};