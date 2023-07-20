#include "worker.hpp"

#include <esp_log.h>
#include <esp_random.h>

#include <espnow_multi.hpp>
#include <utility>

#include "def.hpp"
#include "layout.hpp"
#include "mtx.hpp"
#include "queue.hpp"
#include "util/util.hpp"

namespace meshnow::send {

static constexpr auto TAG = CREATE_TAG("SendWorker");
static constexpr auto MIN_TIMEOUT = pdMS_TO_TICKS(500);

class Sender : public espnow_multi::EspnowSender {
   public:
    void sendCallback(const uint8_t* peer_addr, esp_now_send_status_t status) override {
        // TODO
    }
};

class SendSinkImpl : public SendSink {
   public:
    SendSinkImpl(std::shared_ptr<Sender> sender, SendBehavior behavior, packets::Payload payload, uint32_t id)
        : sender_(std::move(sender)), behavior_(std::move(behavior)), payload_(std::move(payload)), id_(id) {}

    bool accept(const util::MacAddr& next_hop, const util::MacAddr& from, const util::MacAddr& to) override {
        // serialize
        auto buffer = packets::serialize(packets::Packet{id_, from, to, payload_});
        if (espnow_multi::EspnowMulti::getInstance()->send(sender_, next_hop.addr.data(), buffer.data(),
                                                           buffer.size()) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send packet!");
            return false;
        } else {
            ESP_LOGV(TAG, "Sent packet!");
            return true;
        }
    }

    void requeue() override { enqueuePayload(payload_, behavior_, id_); }

   private:
    std::shared_ptr<Sender> sender_;
    SendBehavior behavior_;
    packets::Payload payload_;
    uint32_t id_;
};

void worker_task(bool& should_stop, util::WaitBits& task_waitbits, int send_worker_finished_bit) {
    ESP_LOGI(TAG, "Starting!");

    // create sender
    auto sender = std::make_shared<Sender>();

    while (!should_stop) {
        auto item = popItem(MIN_TIMEOUT);
        if (!item.has_value()) {
            // no packet in time
            continue;
        }

        {
            auto _ = lock();
            SendSinkImpl sink{sender, item->behavior, item->payload, item->id};
            // delegate sending to send behavior
            std::visit([&](auto& behavior) { behavior.send(sink); }, item->behavior);
        }
    }

    ESP_LOGI(TAG, "Stopping!");
    task_waitbits.set(send_worker_finished_bit);
}

}  // namespace meshnow::send