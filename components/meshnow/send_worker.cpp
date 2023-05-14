#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("SendWorker");

static const auto SEND_SUCCESS_BIT = BIT0;
static const auto SEND_FAILED_BIT = BIT1;

void meshnow::SendWorker::enqueuePayload(const meshnow::MAC_ADDR& dest_addr,
                                         std::unique_ptr<meshnow::packets::BasePayload> payload) {
    // TODO use custom delay, don't wait forever (risk of deadlock)
    send_queue_.push_back({dest_addr, std::move(payload)}, portMAX_DELAY);
}

void meshnow::SendWorker::sendFinished(bool successful) {
    // simply use waitbits to notify the waiting thread
    waitbits_.setBits(successful ? SEND_SUCCESS_BIT : SEND_FAILED_BIT);
}

[[noreturn]] void meshnow::SendWorker::run() {
    while (true) {
        // wait forever for the next item in the queue
        auto optional = send_queue_.pop(portMAX_DELAY);
        if (!optional) {
            ESP_LOGE(TAG, "Failed to pop from send queue");
            continue;
        }
        auto& [mac_addr, payload] = *optional;
        meshnow::packets::Packet packet{*payload};
        meshnow::Networking::rawSend(mac_addr, packet.serialize());

        // wait for callback
        // TODO use custom delay, don't wait forever (risk of deadlock)
        auto bits = waitbits_.waitFor(SEND_SUCCESS_BIT | SEND_FAILED_BIT, true, false, portMAX_DELAY);
        if (bits & SEND_SUCCESS_BIT) {
            ESP_LOGI(TAG, "Send successful");
        } else {
            ESP_LOGE(TAG, "Send failed");
        }
    }
}