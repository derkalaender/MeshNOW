#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "constants.hpp"
#include "esp_log.h"
#include "packets.hpp"
#include "queue.hpp"
#include "waitbits.hpp"

namespace meshnow {

struct ReceiveMeta {
    MAC_ADDR src_addr;
    MAC_ADDR dest_addr;
    uint8_t rssi;
};

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
    Networking() : send_waitbits_{}, send_queue_{10}, send_thread_{&Networking::SendWorker, this} {}

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    /**
     * Send callback for ESP-NOW.
     */
    void on_send(const uint8_t* mac_addr, esp_now_send_status_t status);

    /**
     * Receive callback for ESP-NOW.
     */
    void on_receive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len);

    void handleStillAlive(const ReceiveMeta& meta);

    void handleAnyoneThere(const ReceiveMeta& meta);

    void handleIAmHere(const ReceiveMeta& meta);

    void handlePlsConnect(const ReceiveMeta& meta);

    void handleWelcome(const ReceiveMeta& meta);

    void handleNodeConnected(const ReceiveMeta& meta, const packets::NodeConnectedPayload& payload);

    void handleNodeDisconnected(const ReceiveMeta& meta, const packets::NodeDisconnectedPayload& payload);

    void handleMeshUnreachable(const ReceiveMeta& meta);

    void handleMeshReachable(const ReceiveMeta& meta);

    void handleDataAck(const ReceiveMeta& meta, const packets::DataAckPayload& payload);

    void handleDataNack(const ReceiveMeta& meta, const packets::DataNackPayload& payload);

    void handleDataFirst(const ReceiveMeta& meta, const packets::DataFirstPayload& payload);

    void handleDataNext(const ReceiveMeta& meta, const packets::DataNextPayload& payload);

   private:
    /**
     * Sends raw data to the specific device (ESP-NOW wrapper).
     * @param mac_addr the MAC address of the device to send to
     * @param data data to send
     *
     * @note Payloads larger than MAX_RAW_PACKET_SIZE will throw an exception.
     */
    static void raw_send(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data);

    /**
     * Add payload to queue for SendWorker to send.
     *
     * @note Blocks if send queue is full.
     *
     * @param dest_addr MAC address of the immediate node to send the payload to
     * @param payload payload to send
     */
    void enqueue_payload(const MAC_ADDR& dest_addr, std::unique_ptr<meshnow::packets::BasePayload> payload);

    /**
     * Handles the send queue. Sends packets one by one, ensuring they were received by the next node.
     */
    [[noreturn]] void SendWorker();

    /**
     * Communicates a successful/failed payload from the send callback to the SendWorker.
     */
    util::WaitBits send_waitbits_;

    // TODO dont use pair?
    // TODO make priority queue because we want events and stuff first
    util::Queue<std::pair<meshnow::MAC_ADDR, std::unique_ptr<meshnow::packets::BasePayload>>> send_queue_;

    /**
     * Thread the SendWorker runs on.
     */
    std::thread send_thread_;
};

}  // namespace meshnow
