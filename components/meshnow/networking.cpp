#include "networking.hpp"

#include <esp_log.h>

#include <cstdint>

#include "internal.hpp"
#include "state.hpp"

static const char* TAG = CREATE_TAG("Networking");

using meshnow::Networking;

Networking::Networking(std::shared_ptr<NodeState> state) : state_(std::move(state)) {
    // set root mac in layout
    if (state_->isRoot()) {
        std::scoped_lock lock{layout_->mtx};
        layout_->root = layout_->mac;
    }

    // create netif
    if (state_->isRoot()) {
        netif_ = std::make_unique<meshnow::lwip::netif::RootNetif>(send_worker_, layout_);
    } else {
        netif_ = std::make_unique<meshnow::lwip::netif::NodeNetif>(send_worker_, layout_);
    }
}

void Networking::start() {
    ESP_LOGI(TAG, "Starting");

    // init netif
    netif_->init();

    // start both workers
    main_worker_->start();
    send_worker_->start();

    // if we are root, we can also start it already
    if (state_->isRoot()) {
        netif_->start();
    }
}

void Networking::stop() {
    ESP_LOGI(TAG, "Stopping");

    // TODO this will fail an assert (and crash) because of https://github.com/espressif/esp-idf/issues/10664
    // TODO maybe wrap all of networking in yet another thread which we can safely stop ourselves (no jthread)

    // stop both workers
    main_worker_->stop();
    send_worker_->stop();
}
