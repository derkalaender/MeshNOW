#include "worker.hpp"

#include <esp_log.h>

#include "def.hpp"
#include "espnow_multi.hpp"
#include "layout.hpp"
#include "queue.hpp"
#include "util/util.hpp"

namespace meshnow::send {

static constexpr auto TAG = CREATE_TAG("SendWorker");
static constexpr auto MIN_TIMEOUT = pdMS_TO_TICKS(500);

/**
 * Sends packet via ESP-NOW. Interface and impl are separate to achieve low coupling and prevent circular dependency.
 */
class SendSinkImpl : public SendSink, espnow_multi::EspnowInterface {
   public:
    void sendCallback(const uint8_t* peer_addr, esp_now_send_status_t status) override {}

    // not used
    void receiveCallback(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) override {}

    bool accept(const util::MacAddr& dest_addr, const packets::Payload& payload) override {
        // serialize
        auto buffer = packets::serialize(packets::Packet{0, payload});
        if (send(dest_addr.addr.data(), buffer.data(), buffer.size()) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send packet!");
            return false;
        } else {
            ESP_LOGI(TAG, "Sent packet!");
            return true;
        }
    }

   private:
};

void worker_task(bool& should_stop, util::WaitBits& task_waitbits, int send_worker_finished_bit) {
    ESP_LOGI(TAG, "Starting!");

    SendSinkImpl sink;

    while (!should_stop) {
        auto item = popItem(MIN_TIMEOUT);
        if (!item.has_value()) {
            // no packet in time
            continue;
        }

        // delegate sending to send behavior
        item->behavior->send(sink, item->payload);
    }

    ESP_LOGI(TAG, "Stopping!");
    task_waitbits.set(send_worker_finished_bit);
}

}  // namespace meshnow::send