#include "send_worker.hpp"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <utility>

#include "constants.hpp"
#include "error.hpp"
#include "internal.hpp"

static const char* TAG = CREATE_TAG("SendWorker");

static const auto SEND_SUCCESS_BIT = BIT0;
static const auto SEND_FAILED_BIT = BIT1;

// timeout for sending a packet (ms)
static const auto SEND_TIMEOUT = 1000;
// maximum number of retries for sending a packet
static const uint8_t MAX_RETRIES = 5;
// timeout for waiting for an ack (ms)
static const auto ACK_TIMEOUT = 500;
// frequency for checking QoS (ms)
static const auto QOS_CHECK_FREQUENCY = 100;

/**
 * Send raw data via ESP-NOW
 * @param mac_addr address to send to
 * @param data data to send
 */
static void rawSend(const meshnow::MAC_ADDR& mac_addr, const std::vector<uint8_t>& data);

using meshnow::SendWorker;

SendWorker::SendWorker(std::shared_ptr<routing::Layout> layout) : layout_(std::move(layout)) {}

void SendWorker::start() {
    ESP_LOGI(TAG, "Starting!");
    run_thread_ = std::jthread{[this](const std::stop_token& stoken) { runLoop(stoken); }};
    qos_thread_ = std::jthread{[this](const std::stop_token& stoken) { qosChecker(stoken); }};
}

void SendWorker::stop() {
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

void SendWorker::enqueuePayload(const MAC_ADDR& dest_addr, bool resolve, const packets::Payload& payload,
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

void SendWorker::onSend(const uint8_t*, esp_now_send_status_t status) {
    ESP_LOGD(TAG, "Send status: %d", status);
    // set waitbits so we can send again
    waitbits_.setBits(status == ESP_NOW_SEND_SUCCESS ? SEND_SUCCESS_BIT : SEND_FAILED_BIT);
}

void SendWorker::receivedAck(uint8_t seq_num) {
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

void SendWorker::receivedNack(uint8_t seq_num, packets::Nack::Reason reason) {
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

void SendWorker::runLoop(const std::stop_token& stoken) {
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
            std::scoped_lock lock{layout_->mtx};

            // resolve next hop mac
            auto next_hop_mac = routing::resolve(layout_, item.dest_addr);
            if (!next_hop_mac) {
                // couldn't resolve next hop mac
                ESP_LOGD(TAG, "Couldn't resolve next hop mac: " MAC_FORMAT, MAC_FORMAT_ARGS(item.dest_addr));
                item.result_promise.set_value(SendResult{false});
                continue;
            }
            addr = *next_hop_mac;
        } else {
            addr = item.dest_addr;
        }

        ESP_LOGV(TAG, "Sending packet to " MAC_FORMAT, MAC_FORMAT_ARGS(addr));

        rawSend(addr, packets::serialize(item.packet));

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
            item.result_promise.set_value(SendResult{false});
        }
    }
}

void SendWorker::handleSuccess(SendWorker::SendQueueItem&& item, const MAC_ADDR&) {
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

void SendWorker::handleFailure(SendWorker::SendQueueItem&& item, const MAC_ADDR& next_hop) {
    switch (item.qos) {
        case QoS::SINGLE_TRY: {
            item.result_promise.set_value(SendResult{false});
        } break;
        case QoS::WAIT_ACK_TIMEOUT:
        case QoS::NEXT_HOP: {
            std::scoped_lock lock{layout_->mtx};

            // check if the resolved host is still registered
            if (routing::hasNeighbor(layout_, next_hop)) {
                // requeue the item
                send_queue_.push_back(std::move(item), portMAX_DELAY);
            } else {
                // immediately resolve the promise to fail
                item.result_promise.set_value(SendResult{false});
            }
        } break;
    }
}

void SendWorker::qosChecker(const std::stop_token& stoken) {
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

// INTERNAL //

// TODO handle list of peers full
static void add_peer(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGV(TAG, "Adding peer " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    if (esp_now_is_peer_exist(mac_addr.data())) {
        return;
    }
    esp_now_peer_info_t peer_info{};
    peer_info.channel = 0;
    peer_info.encrypt = false;
    peer_info.ifidx = WIFI_IF_STA;
    std::copy(mac_addr.begin(), mac_addr.end(), peer_info.peer_addr);
    CHECK_THROW(esp_now_add_peer(&peer_info));
}

static void rawSend(const meshnow::MAC_ADDR& mac_addr, const std::vector<uint8_t>& data) {
    if (data.size() > meshnow::MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum data size %d", data.size(), meshnow::MAX_RAW_PACKET_SIZE);
        throw meshnow::PayloadTooLargeException();
    }

    // add peer, send, remove peer
    add_peer(mac_addr);
    ESP_LOGV(TAG, "Sending raw data to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), data.data(), data.size()));
    esp_now_del_peer(mac_addr.data());
}