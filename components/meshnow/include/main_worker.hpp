#pragma once

#include <memory>
#include <thread>

#include "constants.hpp"
#include "layout.hpp"
#include "queue.hpp"
#include "send_worker.hpp"
#include "state.hpp"

namespace meshnow {

class MainWorker {
   public:
    MainWorker(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<routing::Layout> layout,
               std::shared_ptr<NodeState> state);
    MainWorker(const MainWorker&) = delete;
    MainWorker& operator=(const MainWorker&) = delete;

    /**
     * Starts the MainWorker.
     */
    void start();

    /**
     * Stops the MainWorker.
     */
    void stop();

    /**
     * Receive callback for ESP-NOW.
     * Pushes items to the queue.
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
     * Main run loop of the network stack.
     * - Queries incoming packets and handles them
     * - Checks if connected neighbors are still alive
     * - Sends still alive beacon itself
     * - Tries to reconnect if not connected
     * @param stoken stop token to interrupt the loop
     */
    void runLoop(const std::stop_token& stoken);

    /**
     * Thread in which the main run loop is executed.
     */
    std::jthread run_thread_;

    /**
     * Queue for incoming packets.
     */
    util::Queue<ReceiveQueueItem> receive_queue_{10};

    /**
     * SendWorker for queueing packets to send.
     */
    std::shared_ptr<SendWorker> send_worker_;

    /**
     * Layout of the network.
     */
    std::shared_ptr<routing::Layout> layout_;

    /**
     * State of the node.
     */
    std::shared_ptr<NodeState> state_;
};

}  // namespace meshnow