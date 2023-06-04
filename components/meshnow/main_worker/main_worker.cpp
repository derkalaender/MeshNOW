#include "main_worker.hpp"

#include <esp_log.h>
#include <freertos/portmacro.h>

#include <array>
#include <ranges>
#include <utility>

#include "hand_shaker.hpp"
#include "internal.hpp"
#include "keep_alive.hpp"
#include "worker_task.hpp"

static const char *TAG = CREATE_TAG("MainWorker");

using meshnow::MainWorker;

meshnow::MainWorker::MainWorker(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<routing::Layout> layout,
                                std::shared_ptr<NodeState> state)
    : send_worker_(std::move(send_worker)), layout_(std::move(layout)), state_(std::move(state)) {}

void MainWorker::start() {
    ESP_LOGI(TAG, "Starting main run loop!");
    run_thread_ = std::jthread{[this](std::stop_token stoken) { runLoop(stoken); }};
}

void MainWorker::stop() {
    ESP_LOGI(TAG, "Stopping main run loop!");
    run_thread_.request_stop();
}

void MainWorker::onReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    ESP_LOGV(TAG, "Received data");

    ReceiveQueueItem item{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), item.from.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), item.to.begin());
    item.rssi = esp_now_info->rx_ctrl->rssi;
    item.data = std::vector<uint8_t>(data, data + data_len);

    // add to receive queue
    receive_queue_.push_back(std::move(item), portMAX_DELAY);
}

template <std::size_t N>
static inline TickType_t calculateTimeout(const std::array<std::reference_wrapper<meshnow::WorkerTask>, N> &tasks) {
    // we want to at least check every 500ms, if not for the stop request
    auto timeout = pdMS_TO_TICKS(500);

    auto now = xTaskGetTickCount();
    // go through every task and check if it has a sooner timeout
    for (auto &&task : tasks) {
        auto next_action = task.get().nextActionAt();
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

void MainWorker::runLoop(const std::stop_token &stoken) {
    keepalive::BeaconSendTask beacon_send{send_worker_, layout_};
    keepalive::RootReachableCheckTask root_reachable_check{state_, layout_};
    keepalive::NeighborsAliveCheckTask neighbors_alive_check{send_worker_, state_, layout_};

    HandShaker hand_shaker{send_worker_, state_, layout_};

    std::array<std::reference_wrapper<WorkerTask>, 4> tasks{beacon_send, neighbors_alive_check, root_reachable_check,
                                                            hand_shaker};

    PacketHandler packet_handler{send_worker_,        state_, layout_, hand_shaker, neighbors_alive_check,
                                 root_reachable_check};

    auto lastLoopRun = xTaskGetTickCount();

    while (!stoken.stop_requested()) {
        // calculate timeout
        auto timeout = calculateTimeout(tasks);

        ESP_LOGV(TAG, "Next action in at most %lu ticks", timeout);

        // get next packet from receive queue
        auto receive_item = receive_queue_.pop(timeout);
        if (receive_item) {
            // if valid, try to parse
            auto packet = packets::deserialize(receive_item->data);
            if (packet) {
                // if deserialization worked, give packet to packet handler
                ReceiveMeta meta{receive_item->from, receive_item->to, receive_item->rssi, packet->id};
                packet_handler.handlePacket(meta, packet->payload);
            }
        }

        // perform tasks
        for (auto &&task : tasks) {
            task.get().performAction();
        }

        // wait at least one tick to avoid triggering the watchdog
        xTaskDelayUntil(&lastLoopRun, 1);
    }

    ESP_LOGI(TAG, "Exiting main run loop!");
}
