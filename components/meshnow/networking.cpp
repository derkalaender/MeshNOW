#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "hand_shaker.hpp"
#include "internal.hpp"
#include "packets.hpp"
#include "receive_meta.hpp"
#include "state.hpp"

static const char* TAG = CREATE_TAG("Networking");

meshnow::Networking::Networking(std::shared_ptr<NodeState> state)
    : send_worker_(std::make_shared<SendWorker>(layout_)),
      main_worker_(std::make_shared<MainWorker>(send_worker_, layout_, state)) {}

void meshnow::Networking::start() {
    // start both workers
    main_worker_->start();
    send_worker_->start();
}

void meshnow::Networking::stop() {
    ESP_LOGI(TAG, "Stopping main run loop!");

    // TODO this will fail an assert (and crash) because of https://github.com/espressif/esp-idf/issues/10664
    // TODO maybe wrap all of networking in yet another thread which we can safely stop ourselves (no jthread)

    // stop both workers
    main_worker_->stop();
    send_worker_->stop();
}
