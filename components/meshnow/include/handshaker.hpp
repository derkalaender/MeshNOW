#pragma once

#include <thread>
#include <vector>
#include <waitbits.hpp>

#include "constants.hpp"

namespace meshnow {

class Networking;

/**
 * Whenever disconnected from a parent, this thread tries to connect to the best parent by continuously sending connect
 * requests.
 */
class Handshaker {
   public:
    explicit Handshaker(Networking& networking);

    /**
     * Notify the Handshaker that the node is ready to connect to a parent.
     */
    void readyToConnect();

    /**
     * Notify the Handshaker that it should stop trying to connect to a parent.
     */
    void stopConnecting();

    /**
     * Notify the Handshaker that a possible parent was found.
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

    util::WaitBits waitbits_{};

    std::thread thread_;

    std::mutex mtx_{};

    // TODO place magic constant in internal.hpp
    std::vector<ParentInfo> parent_infos_{};

    TickType_t first_parent_found_time_{0};
};

}  // namespace meshnow