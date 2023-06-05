#include "networking.hpp"

#include <esp_log.h>

#include <cstdint>

#include "internal.hpp"
#include "state.hpp"

static const char* TAG = CREATE_TAG("Networking");

using meshnow::Networking;

Networking::Networking(const std::shared_ptr<NodeState>& state)
    : send_worker_(std::make_shared<SendWorker>(layout_)),
      main_worker_(std::make_shared<MainWorker>(send_worker_, layout_, state)) {
    // set root mac in layout
    if (state->isRoot()) {
        std::scoped_lock lock{layout_->mtx};
        layout_->root = layout_->mac;
    }
}

void Networking::start() {
    // start both workers
    main_worker_->start();
    send_worker_->start();
}

void Networking::stop() {
    ESP_LOGI(TAG, "Stopping main run loop!");

    // TODO this will fail an assert (and crash) because of https://github.com/espressif/esp-idf/issues/10664
    // TODO maybe wrap all of networking in yet another thread which we can safely stop ourselves (no jthread)

    // stop both workers
    main_worker_->stop();
    send_worker_->stop();
}
