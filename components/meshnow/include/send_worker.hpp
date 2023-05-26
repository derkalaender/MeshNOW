#pragma once

#include <queue.hpp>
#include <thread>
#include <waitbits.hpp>

#include "constants.hpp"
#include "packets.hpp"

namespace meshnow {

class Networking;

/**
 * Thread that handles sending payloads by queueing them and working them off one by one.
 */
class SendWorker {
   public:
    SendWorker() = default;

    SendWorker(const SendWorker&) = delete;
    SendWorker& operator=(const SendWorker&) = delete;

    /**
     * Starts the SendWorker.
     */
    void start();

    /**
     * Stops the SendWorker.
     */
    void stop();

    /**
     * Add packet to the send queue.
     *
     * @note Blocks if send queue is full.
     *
     * @param dest_addr MAC address of the immediate node to send the packet to
     * @param packet packet to send
     */
    void enqueuePacket(const MAC_ADDR& dest_addr, packets::Packet packet);

    /**
     * Notify the SendWorker that the previous payload was sent.
     */
    void sendFinished(bool successful);

   private:
    struct SendQueueItem {
        MAC_ADDR dest_addr{};
        packets::Packet packet{};
    };

    /**
     * Loop of the SendWorker thread.
     * @param stoken stop token to interrupt the loop
     */
    void runLoop(std::stop_token stoken);

    /**
     * Communicates a successful/failed payload from the send callback to the thread.
     */
    util::WaitBits waitbits_{};

    // TODO make priority queue because we want events and stuff first
    // TODO extract max_items to constants.hpp
    util::Queue<SendQueueItem> send_queue_{10};

    std::jthread run_thread_{};
};

}  // namespace meshnow