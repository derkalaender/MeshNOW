#include "send_worker.hpp"

#include <esp_log.h>

#include <utility>

#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("SendWorker");

static const auto SEND_SUCCESS_BIT = BIT0;
static const auto SEND_FAILED_BIT = BIT1;

void meshnow::SendWorker::start() {
    ESP_LOGI(TAG, "Starting!");
    run_thread_ = std::jthread{[this](std::stop_token stoken) { runLoop(stoken); }};
}

void meshnow::SendWorker::stop() {
    ESP_LOGI(TAG, "Stopping!");
    run_thread_.request_stop();
}

void meshnow::SendWorker::enqueuePacket(const MAC_ADDR& dest_addr, meshnow::packets::Packet packet) {
    // TODO use custom delay, don't wait forever (risk of deadlock)
    send_queue_.push_back({dest_addr, std::move(packet)}, portMAX_DELAY);
}

void meshnow::SendWorker::sendFinished(bool successful) {
    // simply use waitbits to notify the waiting thread
    waitbits_.setBits(successful ? SEND_SUCCESS_BIT : SEND_FAILED_BIT);
}

void meshnow::SendWorker::runLoop(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // wait forever for the next item in the queue
        auto optional = send_queue_.pop(portMAX_DELAY);
        if (!optional) {
            ESP_LOGE(TAG, "Failed to pop from send queue");
            continue;
        }
        auto& [mac_addr, packet] = *optional;
        meshnow::Networking::rawSend(mac_addr, meshnow::packets::serialize(packet));

        // wait for callback
        // TODO use custom delay, don't wait forever (risk of deadlock)
        auto bits = waitbits_.waitFor(SEND_SUCCESS_BIT | SEND_FAILED_BIT, true, false, portMAX_DELAY);
        if (bits & SEND_SUCCESS_BIT) {
            ESP_LOGD(TAG, "Send successful");
        } else {
            ESP_LOGD(TAG, "Send failed");
        }
    }

    // TODO cleanup
}
