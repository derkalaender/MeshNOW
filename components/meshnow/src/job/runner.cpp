#include "runner.hpp"

#include <esp_log.h>

#include "fragment_gc.hpp"
#include "freertos/portmacro.h"
#include "hand_shaker.hpp"
#include "job.hpp"
#include "keep_alive.hpp"
#include "packet_handler_new.hpp"
#include "receive/queue.hpp"
#include "util/util.hpp"
#include "util/waitbits.hpp"

namespace meshnow::job {

static const char* TAG = CREATE_TAG("JobRunner");
static constexpr auto MIN_TIMEOUT = pdMS_TO_TICKS(5000);

using JobList = std::initializer_list<std::reference_wrapper<Job>>;

static TickType_t calculateTimeout(JobList jobs) {
    // we want to at least check every 500ms, if not for the stop request
    auto timeout = MIN_TIMEOUT;

    auto now = xTaskGetTickCount();
    // go through every task and check if it has a sooner timeout
    for (auto job : jobs) {
        auto next_action = job.get().nextActionAt();
        TickType_t this_timeout;
        if (next_action == portMAX_DELAY) {
            // in case of the maximum delay, we don't want to subtract now, as it is assumed the task never wants to run
            this_timeout = portMAX_DELAY;
        } else if (next_action > now) {
            this_timeout = next_action - now;
        } else {
            // run immediately
            this_timeout = 0;
        }

        // update timeout with potentially sooner timeout
        if (this_timeout < timeout) {
            timeout = this_timeout;
        }

        // break early
        if (timeout == 0) {
            break;
        }
    }

    return timeout;
}

void job::runner_task(bool& should_stop, util::WaitBits& task_waitbits, int job_runner_finished_bit) {
    ESP_LOGI(TAG, "Starting!");

    HandShaker hand_shaker;
    FragmentGCJob fragment_gc;
    StatusSendJob status_send;
    keepalive::UnreachableTimeoutJob root_reachable_check;
    keepalive::NeighborCheckJob neighbors_alive_check;

    JobList jobs{status_send, root_reachable_check, neighbors_alive_check, hand_shaker, fragment_gc};

    PacketHandler packet_handler;

    auto lastLoopRun = xTaskGetTickCount();

    while (!should_stop) {
        // calculate timeout
        auto timeout = calculateTimeout(jobs);

        ESP_LOGV(TAG, "Next action in at most %lu ticks", timeout);

        // get next packet from receive queue
        auto receive_item = receive::pop(timeout);
        if (receive_item) {
            // handle packet
            packet_handler.handlePacket(receive_item->from, receive_item->packet);
        }

        // perform tasks
        for (auto job : jobs) {
            job.get().performAction();
        }

        // wait at least one tick to avoid triggering the watchdog
        xTaskDelayUntil(&lastLoopRun, 1);
    }

    ESP_LOGI(TAG, "Stopping!");

    // finished
    task_waitbits.set(job_runner_finished_bit);
}

}  // namespace meshnow::job