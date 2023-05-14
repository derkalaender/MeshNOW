#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("ConnectionInitiator");

static const auto READY_BIT = BIT0;

// Frequency at which to send connection beacon (ms)
static const auto CONNECTION_FREQ_MS = 500;

void meshnow::ConnectionInitiator::readyToConnect() { waitbits_.setBits(READY_BIT); }

void meshnow::ConnectionInitiator::stopConnecting() { waitbits_.clearBits(READY_BIT); }

[[noreturn]] void meshnow::ConnectionInitiator::run() {
    while (true) {
        // wait to be allowed to initiate a connection
        auto bits = waitbits_.waitFor(READY_BIT, false, true, portMAX_DELAY);
        if (!(bits & READY_BIT)) {
            ESP_LOGE(TAG, "Failed to wait for ready bit");
            continue;
        }

        auto last_tick = xTaskGetTickCount();

        // send anyone there beacon
        ESP_LOGI(TAG, "Sending anyone there beacon");
        networking_.send_worker_.enqueuePayload(meshnow::BROADCAST_MAC_ADDR,
                                                std::make_unique<packets::AnyoneTherePayload>());

        // wait for the next cycle
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(CONNECTION_FREQ_MS));
    }
}