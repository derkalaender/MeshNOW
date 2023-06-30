#pragma once

#include <esp_err.h>
#include <freertos/portmacro.h>

#include <optional>

#include "packets.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::receive {

struct Item {
    util::MacAddr from;
    util::MacAddr to;
    packets::Packet packet;
};

/**
 * Initializes receive queue.
 */
esp_err_t init();

/**
 * Deinitializes receive queue.
 */
void deinit();

/**
 * Pushes a new item to the receive queue.
 *
 * @param item Item to push.
 */
void push(const Item& item);

/**
 * Pops an item from the receive queue.
 *
 * @param item Item to popPacket.
 * @param timeout Timeout in ticks.
 */
std::optional<Item> pop(TickType_t timeout);

}  // namespace meshnow::receive