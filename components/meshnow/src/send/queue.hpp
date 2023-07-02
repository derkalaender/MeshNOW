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
    packets::Packet packet;
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
 * Enqueues a new packet to be sent.
 * @param dest_addr Where to send the packet
 * @param packet The packet to send
 * @param resolve If true, resolves the packet using the internal routing logic, otherwise sends it directly via native
 * ESP-NOW API
 * @param one_shot If true, only tries to send the packet once, otherwise tries as until success or the next hop node is
 * no longer reachable
 * @param priority If true, the packet is sent before all other packets in the queue
 */
void enqueuePacket(util::MacAddr dest_addr, packets::Packet packet, bool resolve, bool one_shot, bool priority);

std::optional<Item> popPacket(TickType_t timeout);

}  // namespace meshnow::send