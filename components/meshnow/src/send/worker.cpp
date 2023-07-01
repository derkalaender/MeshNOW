#include <esp_log.h>

#include "layout.hpp"
#include "queue.hpp"
#include "util/util.hpp"
#include "worker.hpp"

namespace meshnow::send {

static constexpr auto TAG = CREATE_TAG("SendWorker");
static constexpr auto MIN_TIMEOUT = pdMS_TO_TICKS(500);

void worker_task(bool& should_stop, util::WaitBits& task_waitbits, int send_worker_finished_bit) {
    ESP_LOGI(TAG, "Starting!");

    while (!should_stop) {
        auto item = popPacket(MIN_TIMEOUT);
        if (!item.has_value()) {
            // no packet in time
            continue;
        }

        util::MacAddr dest_addr;
        if (item->resolve) {
            // resolve mac address
            auto resolved = routing::resolve(item->dest_addr);
            if (!resolved.has_value()) {
                // could not resolve mac address
                ESP_LOGW(TAG, "Could not resolve mac address for " MACSTR, MAC2STR(item->dest_addr));
                continue;
            }
            dest_addr = resolved.value();
        } else {
            // use mac address directly
            dest_addr = item->dest_addr;
        }

        ESP_LOGV(TAG, "Sending packet to " MACSTR, MAC2STR(dest_addr));

        // TODO send function

        // TODO handle failure and retry
    }

    ESP_LOGI(TAG, "Stopping!");
}

}  // namespace meshnow::send