#pragma once

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <queue.hpp>
#include <thread>
#include <vector>
#include <waitbits.hpp>

#include "constants.hpp"
#include "packets.hpp"
#include "routing.hpp"
#include "state.hpp"

namespace meshnow {

struct ReceiveMeta {
    MAC_ADDR src_addr;
    MAC_ADDR dest_addr;
    uint16_t seq_num;
    int rssi;
};

/**
 * Thread that handles sending payloads by queueing them and working them off one by one.
 */
class SendWorker {
   public:
    explicit SendWorker(Networking& networking)
        // TODO extract max_items to constants.hpp
        : networking_{networking}, waitbits_{}, send_queue_{10}, thread_{&SendWorker::run, this} {}

    /**
     * Add packet to the send queue.
     *
     * @note Blocks if send queue is full.
     *
     * @param dest_addr MAC address of the immediate node to send the packet to
     * @param packet packet to send
     */
    void enqueuePacket(const MAC_ADDR& dest_addr, meshnow::packets::Packet packet);

    /**
     * Notify the SendWorker that the previous payload was sent.
     */
    void sendFinished(bool successful);

   private:
    struct SendQueueItem {
        MAC_ADDR dest_addr{};
        meshnow::packets::Packet packet{};
    };

    [[noreturn]] void run();

    // TODO unused
    Networking& networking_;

    /**
     * Communicates a successful/failed payload from the send callback to the thread.
     */
    util::WaitBits waitbits_;

    // TODO make priority queue because we want events and stuff first
    util::Queue<SendQueueItem> send_queue_;

    std::thread thread_;
};

/**
 * Whenever disconnected from a parent, this thread tries to connect to the best parent by continuously sending connect
 * requests.
 */
class ConnectionInitiator {
   public:
    explicit ConnectionInitiator(Networking& networking);

    /**
     * Notify the ConnectionInitiator that the node is ready to connect to a parent.
     */
    void readyToConnect();

    /**
     * Notify the ConnectionInitiator that it should stop trying to connect to a parent.
     */
    void stopConnecting();

    /**
     * Notify the ConnectionInitiator that a possible parent was found.
     * @param mac_addr the MAC address of the parent
     * @param rssi rssi of the parent
     */
    void foundParent(const MAC_ADDR& mac_addr, int rssi);

    /**
     * Reject a possible parent.
     * @param mac_addr the MAC address of the parent
     */
    void reject(MAC_ADDR mac_addr);

   private:
    struct ParentInfo {
        MAC_ADDR mac_addr;
        int rssi;
    };

    [[noreturn]] void run();

    void tryConnect();

    void awaitVerdict();

    Networking& networking_;

    util::WaitBits waitbits_;

    std::thread thread_;

    std::mutex mtx_;

    // TODO place magic constant in internal.hpp
    std::vector<ParentInfo> parent_infos_;

    TickType_t first_parent_found_time_;
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
    explicit Networking(NodeState& state)
        : state_{state}, send_worker_{*this}, conn_initiator_{*this}, routing_info_{queryThisMac()} {}

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    static meshnow::MAC_ADDR queryThisMac();

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

    void handle(const ReceiveMeta& meta, const packets::StillAlive& p);

    void handle(const ReceiveMeta& meta, const packets::AnyoneThere& p);

    void handle(const ReceiveMeta& meta, const packets::IAmHere& p);

    void handle(const ReceiveMeta& meta, const packets::PlsConnect& p);

    void handle(const ReceiveMeta& meta, const packets::Verdict& p);

    void handle(const ReceiveMeta& meta, const packets::NodeConnected& p);

    void handle(const ReceiveMeta& meta, const packets::NodeDisconnected& p);

    void handle(const ReceiveMeta& meta, const packets::MeshUnreachable& p);

    void handle(const ReceiveMeta& meta, const packets::MeshReachable& p);

    void handle(const ReceiveMeta& meta, const packets::Ack& p);

    void handle(const ReceiveMeta& meta, const packets::Nack& p);

    void handle(const ReceiveMeta& meta, const packets::LwipDataFirst& p);

    void handle(const ReceiveMeta& meta, const packets::CustomDataFirst& p);

    void handle(const ReceiveMeta& meta, const packets::LwipDataNext& p);

    void handle(const ReceiveMeta& meta, const packets::CustomDataNext& p);

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

    SendWorker send_worker_;

    ConnectionInitiator conn_initiator_;

    meshnow::routing::RoutingInfo routing_info_;

    friend SendWorker;
    friend ConnectionInitiator;
};

}  // namespace meshnow
