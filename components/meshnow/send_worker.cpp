#include "send_worker.hpp"

#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <mutex>
#include <utility>

#include "internal.hpp"
#include "networking.hpp"

static const char* TAG = CREATE_TAG("SendWorker");

static const auto SEND_SUCCESS_BIT = BIT0;
static const auto SEND_FAILED_BIT = BIT1;

// timeout for sending a packet (ms)
static const auto SEND_TIMEOUT = 10 * 1000;
// maximum number of retries for sending a packet
static const uint8_t MAX_RETRIES = 5;
// timeout for waiting for an ack (ms)
static const auto ACK_TIMEOUT = 500;
// frequency for checking QoS (ms)
static const auto QOS_CHECK_FREQUENCY = 100;

void meshnow::SendWorker::start() {
    ESP_LOGI(TAG, "Starting!");
    run_thread_ = std::jthread{[this](const std::stop_token& stoken) { runLoop(stoken); }};
    qos_thread_ = std::jthread{[this](const std::stop_token& stoken) { qosChecker(stoken); }};
}

void meshnow::SendWorker::stop() {
    ESP_LOGI(TAG, "Stopping!");
    run_thread_.request_stop();
    qos_thread_.request_stop();

    // cleanup //

    // resolve all qos promises and clear container
    for (auto& item : qos_vector_) {
        item.item.result_promise.set_value(SendResult{false});
    }
    qos_vector_.clear();

    // clear send queue
    send_queue_.clear();

    // clear waitbits
    waitbits_.clearBits(SEND_SUCCESS_BIT | SEND_FAILED_BIT);
}

void meshnow::SendWorker::enqueuePayload(const MAC_ADDR& dest_addr, bool resolve, const packets::Payload& payload,
                                         SendPromise&& result_promise, bool priority, QoS qos) {
    // create packet
    // TODO don't use magic multiple times
    packets::Packet packet{esp_random(), payload};

    // TODO use custom delay, don't wait forever (risk of deadlock)
    SendQueueItem item{dest_addr, resolve, packet, std::move(result_promise), qos, 0};
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

void meshnow::SendWorker::receivedAck(uint8_t seq_num) {
    std::scoped_lock lock{qos_mutex_};

    // go through QoS list and find the item with matching sequence number
    auto item = std::find_if(qos_vector_.begin(), qos_vector_.end(),
                             [seq_num](const QoSVectorItem& item) { return item.item.packet.id == seq_num; });
    if (item == qos_vector_.end()) return;  // not found

    // resolve promise to ok
    item->item.result_promise.set_value(SendResult{true});
    // remove item from QoS vector
    qos_vector_.erase(item);
}

void meshnow::SendWorker::receivedNack(uint8_t seq_num, packets::Nack::Reason reason) {
    std::scoped_lock lock{qos_mutex_};

    // go through QoS list and find the item with matching sequence number
    auto item = std::find_if(qos_vector_.begin(), qos_vector_.end(),
                             [seq_num](const QoSVectorItem& item) { return item.item.packet.id == seq_num; });
    if (item == qos_vector_.end()) return;  // not found

    // immediately fail the promise if the reason is NOT_FOUND
    if (reason == packets::Nack::Reason::NOT_FOUND) {
        // immediately resolve the promise to fail
        item->item.result_promise.set_value(SendResult{false});
    } else {
        // update the retries counter
        item->item.retries++;
        // requeue the item
        send_queue_.push_back(std::move(item->item), portMAX_DELAY);
    }

    // remove the item from the QoS vector as we either don't need it anymore or the send worker will re-add it
    qos_vector_.erase(item);
}

void meshnow::SendWorker::runLoop(const std::stop_token& stoken) {
    while (!stoken.stop_requested()) {
        // wait forever for the next item in the queue
        auto optional = send_queue_.pop(portMAX_DELAY);
        if (!optional) {
            ESP_LOGE(TAG, "Failed to pop from send queue");
            continue;
        }
        SendQueueItem item{std::move(*optional)};

        assert(!(item.resolve && item.qos == QoS::SINGLE_TRY) && "Cannot resolve with no QoS");

        MAC_ADDR addr;
        if (item.resolve) {
            // resolve next hop mac
            auto next_hop_mac = router_.resolve(item.dest_addr);
            if (!next_hop_mac) {
                // couldn't resolve next hop mac
                ESP_LOGD(TAG, "Couldn't resolve next hop mac");
                item.result_promise.set_value(SendResult{false});
                continue;
            }
            addr = *next_hop_mac;
        } else {
            addr = item.dest_addr;
        }

        meshnow::Networking::rawSend(addr, meshnow::packets::serialize(item.packet));

        // wait for callback
        auto bits = waitbits_.waitFor(SEND_SUCCESS_BIT | SEND_FAILED_BIT, true, false, SEND_TIMEOUT);

        if (bits & SEND_SUCCESS_BIT) {
            ESP_LOGD(TAG, "Send successful");
            handleSuccess(std::move(item), addr);
        } else if (bits & SEND_FAILED_BIT) {
            ESP_LOGD(TAG, "Send failed");
            handleFailure(std::move(item), addr);
        } else {
            ESP_LOGW(TAG,
                     "Sending of packet timed out.\n"
                     "This should never happen in regular usage and hints at a problem with either ESP-NOW, hardware "
                     "or the rest of your code.\n"
                     "To avoid any potential deadlocks, the packet that was tried to be sent will be dropped "
                     "regardless of QoS.");
        }
    }
}

void meshnow::SendWorker::handleSuccess(meshnow::SendWorker::SendQueueItem&& item, const MAC_ADDR&) {
    switch (item.qos) {
        case QoS::SINGLE_TRY:
        case QoS::NEXT_HOP: {
            // in these cases, we are done and can successfully resolve the promise
            item.result_promise.set_value(SendResult{true});
        } break;
        case QoS::WAIT_ACK_TIMEOUT: {
            // if we haven't exhausted the maximum number of retries, add the item to the QoS vector
            if (item.retries < MAX_RETRIES) {
                std::scoped_lock lock{qos_mutex_};
                qos_vector_.push_back(QoSVectorItem{std::move(item), xTaskGetTickCount()});
            } else {
                // immediately resolve the promise to fail
                item.result_promise.set_value(SendResult{false});
            }
        } break;
    }
}

void meshnow::SendWorker::handleFailure(meshnow::SendWorker::SendQueueItem&& item, const MAC_ADDR& next_hop) {
    switch (item.qos) {
        case QoS::SINGLE_TRY: {
            item.result_promise.set_value(SendResult{false});
        } break;
        case QoS::WAIT_ACK_TIMEOUT:
        case QoS::NEXT_HOP: {
            // check if the resolved host is still registered
            if (router_.hasNeighbor(next_hop)) {
                // requeue the item
                send_queue_.push_back(std::move(item), portMAX_DELAY);
            } else {
                // immediately resolve the promise to fail
                item.result_promise.set_value(SendResult{false});
            }
        } break;
    }
}

void meshnow::SendWorker::qosChecker(const std::stop_token& stoken) {
    while (!stoken.stop_requested()) {
        auto now = xTaskGetTickCount();

        {
            std::scoped_lock lock{qos_mutex_};

            // remove all items from QoS vector that have timed out and instead requeue them for sending
            std::erase_if(qos_vector_, [now, this](QoSVectorItem& item) {
                if (now - item.sent_time > pdMS_TO_TICKS(ACK_TIMEOUT)) {
                    item.item.retries++;
                    send_queue_.push_back(std::move(item.item), portMAX_DELAY);
                    return true;
                }
                return false;
            });
        }

        vTaskDelay(pdMS_TO_TICKS(QOS_CHECK_FREQUENCY));
    }
}
