#pragma once

#include <esp_err.h>
#include <freertos/portmacro.h>

#include <memory>
#include <optional>

#include "def.hpp"
#include "packets.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::send {

struct Item {
    packets::Payload payload;
    std::unique_ptr<SendBehavior> behavior;
};

/**
 * Initializes the send queue.
 */
esp_err_t init();

/**
 * Deinitializes the send queue.
 */
void deinit();

/**
 * Enqueues a new payload to be sent.
 * @param packet The payload to send
 * @param behavior The behavior to use for sending
 * @param priority If true, the payload is sent before all others in the queue
 */
void enqueuePayload(const packets::Payload& payload, std::unique_ptr<SendBehavior> behavior, bool priority);

std::optional<Item> popItem(TickType_t timeout);

}  // namespace meshnow::send