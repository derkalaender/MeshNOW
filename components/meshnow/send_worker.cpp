#include "send_worker.hpp"

#include <esp_log.h>

#include <utility>

#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("SendWorker");

static const auto SEND_SUCCESS_BIT = BIT0;
static const auto SEND_FAILED_BIT = BIT1;

// timeout for sending a packet (ms)
static const auto SEND_TIMEOUT = 10 * 1000;

void meshnow::SendWorker::start() {
    ESP_LOGI(TAG, "Starting!");
    run_thread_ = std::jthread{[this](std::stop_token stoken) { runLoop(stoken); }};
}

void meshnow::SendWorker::stop() {
    ESP_LOGI(TAG, "Stopping!");
    run_thread_.request_stop();
}

void meshnow::SendWorker::enqueuePacket(const MAC_ADDR& dest_addr, meshnow::packets::Packet packet,
                                        SendPromise&& result_promise, bool priority, QoS qos) {
    // TODO use custom delay, don't wait forever (risk of deadlock)
    SendQueueItem item{std::move(packet), std::move(result_promise), dest_addr, qos};
    if (priority) {
        send_queue_.push_front(std::move(item), portMAX_DELAY);
    } else {
        send_queue_.push_back(std::move(item), portMAX_DELAY);
    }
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
        SendQueueItem item{std::move(*optional)};

        meshnow::Networking::rawSend(item.dest_addr, meshnow::packets::serialize(item.packet));

        // wait for callback
        auto bits = waitbits_.waitFor(SEND_SUCCESS_BIT | SEND_FAILED_BIT, true, false, SEND_TIMEOUT);

        if (bits & SEND_SUCCESS_BIT) {
            ESP_LOGD(TAG, "Send successful");

            // handle QoS
            if (item.qos == QoS::FIRE_AND_FORGET) {
                item.result_promise.set_value(SendResult{true});
            } else {
                // TODO
            }
        } else if (bits & SEND_FAILED_BIT) {
            ESP_LOGD(TAG, "Send failed");

            // TODO handle qos and requeue if the dest node is still registered
            item.result_promise.set_value(SendResult{false});
        } else {
            ESP_LOGW(TAG,
                     "Sending of packet timed out.\n"
                     "This should never happen in regular usage and hints at a problem with either ESP-NOW, hardware "
                     "or the rest of your code.\n"
                     "To avoid any potential deadlocks, the packet that was tried to be sent will be dropped "
                     "regardless of QoS.");
        }
    }

    // TODO cleanup
}
