#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "constants.hpp"
#include "packet_handler.hpp"
#include "receive_meta.hpp"
#include "router.hpp"
#include "send_worker.hpp"
#include "state.hpp"

namespace meshnow {

/**
 * Handles networking.
 * <br>
 * Terminology:<br>
 * - Root: The root node is the only one potentially connected to a router and to that extend the internet. Coordinates
 * the whole network. Should be connected to a power source.<br>
 * - Node: If not otherwise specified, a "node" refers to any node that is not the root. Can be a leaf node or a branch
 * node.<br>
 * - Parent: Each node (except for the root) has exactly one parent node.<br>
 * - Child: Each node can have multiple children.<br>
 */
class Networking {
   public:
    explicit Networking(NodeState& state);

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    /**
     * Start the networking stack.
     */
    void start();

    /**
     * Send callback for ESP-NOW.
     */
    void onSend(const uint8_t* mac_addr, esp_now_send_status_t status);

    /**
     * Receive callback for ESP-NOW.
     */
    void onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len);

   private:
    /**
     * Sends raw data to the specific device (ESP-NOW wrapper).
     * @param mac_addr the MAC address of the device to send to
     * @param data data to send
     *
     * @note Payloads larger than MAX_RAW_PACKET_SIZE will throw an exception.
     */
    static void rawSend(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data);

    /**
     * Reference to the current state of the node. Used to know if we are connected or not, etc.
     */
    NodeState& state_;

    SendWorker send_worker_{};

    routing::Router router_{state_.isRoot()};

    std::vector<std::shared_ptr<PacketHandler>> packet_handlers_{};

    friend SendWorker;
};

}  // namespace meshnow
