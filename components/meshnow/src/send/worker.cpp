#include "worker.hpp"

#include <esp_log.h>

#include "def.hpp"
#include "layout.hpp"
#include "queue.hpp"
#include "util/util.hpp"

namespace meshnow::send {

static constexpr auto TAG = CREATE_TAG("SendWorker");
static constexpr auto MIN_TIMEOUT = pdMS_TO_TICKS(500);

/**
 * Sends packet via ESP-NOW. Interface and impl are separate to achieve low coupling and prevent circular dependency.
 */
class SendSinkImpl : public SendSink {
   public:
    bool accept(const util::MacAddr& dest_addr, const packets::Packet& packet) override {
        // TODO implement
        return true;
    }
};

void worker_task(bool& should_stop, util::WaitBits& task_waitbits, int send_worker_finished_bit) {
    ESP_LOGI(TAG, "Starting!");

    SendSinkImpl sink;

    while (!should_stop) {
        auto item = popPacket(MIN_TIMEOUT);
        if (!item.has_value()) {
            // no packet in time
            continue;
        }

        // delegate sending to send behavior
        item->behavior->send(sink, item->packet);
    }

    ESP_LOGI(TAG, "Stopping!");
}

}  // namespace meshnow::send