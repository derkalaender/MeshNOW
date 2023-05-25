#pragma once

#include <freertos/portmacro.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "constants.hpp"
#include "handshaker.hpp"
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
     * Stop the networking stack.
     */
    void stop();

    /**
     * Send callback for ESP-NOW.
     */
    void onSend(const uint8_t* mac_addr, esp_now_send_status_t status);

    /**
     * Receive callback for ESP-NOW.
     */
    void onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len);

   private:
    struct ReceiveQueueItem {
        MAC_ADDR from;
        MAC_ADDR to;
        Buffer data;
        int rssi;
    };

    /**
     * Sends raw data to the specific device (ESP-NOW wrapper).
     * @param mac_addr the MAC address of the device to send to
     * @param data data to send
     *
     * @note Payloads larger than MAX_RAW_PACKET_SIZE will throw an exception.
     */
    static void rawSend(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data);

    /**
     * Main run loop of the network stack.
     * - Queries incoming packets and handles them
     * - Checks if connected neighbors are still alive
     * - Sends still alive beacon itself
     * - Tries to reconnect if not connected
     * @param stoken stop token to interrupt the loop
     */
    void runLoop(const std::stop_token& stoken);

    /**
     * Calculates the minimum timeout until the next action should be performed.
     * This is used as the timeout for waiting for a new packet.
     * @return the timeout value
     */
    TickType_t nextActionIn() const;

    /**
     * Reference to the current state of the node. Used to know if we are connected or not, etc.
     */
    NodeState& state_;

    routing::Router router_{state_.isRoot()};

    SendWorker send_worker_{};

    std::jthread run_thread_{};

    packets::PacketHandler packet_handler_;

    Handshaker handshaker_;

    util::Queue<ReceiveQueueItem> receive_queue_{10};

    friend class SendWorker;
    friend class packets::PacketHandler;
};

}  // namespace meshnow
