#pragma once

#include <freertos/portmacro.h>

#include <memory>
#include <optional>
#include <vector>

#include "constants.hpp"
#include "jobs/job.hpp"
#include "now_lwip/netif.hpp"
#include "send/worker.hpp"
#include "state.hpp"

namespace meshnow {

/**
 * Whenever disconnected from a parent, this thread tries to should_connect to the best parent by continuously sending
 * should_connect requests.
 */
class HandShaker : public Job {
   public:
    explicit HandShaker(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<NodeState> state,
                        std::shared_ptr<routing::Layout> layout, std::shared_ptr<lwip::netif::Netif> netif);

    TickType_t nextActionAt() const noexcept override;

    void performAction() override;

    /**
     * Remove all found parents, prepare to start searching for parents again.
     */
    void reset();

    // Connection requesting methods //

    /**
     * Notify the Handshaker that a possible parent was found.
     * @param mac_addr the MAC address of the parent
     * @param rssi rssi of the parent
     */
    void foundPotentialParent(const MAC_ADDR& mac_addr, int rssi);

    /**
     * Notify the Handshaker that a parent responded to a should_connect request.
     * @param mac_addr the MAC address of the parent
     * @param accept whether the parent accepted the should_connect request
     */
    void receivedConnectResponse(const MAC_ADDR& mac_addr, bool accept, std::optional<MAC_ADDR> root_mac_addr);

    // Connection responding methods //

    /**
     * Handle a search probe from a child.
     * @param mac_addr MAC address of the child
     */
    void receivedSearchProbe(const MAC_ADDR& mac_addr);

    /**
     * Handle a should_connect request from a child.
     * @param mac_addr MAC address of the child
     */
    void receivedConnectRequest(const MAC_ADDR& mac_addr);

   private:
    struct ParentInfo {
        MAC_ADDR mac_addr;
        int rssi;
    };

    /**
     * Sends a beacon to search for nearby parents willing to accept this node as a child.
     */
    void sendSearchProbe();

    /**
     * Answers a search probe with a reply.
     * @param mac_addr the MAC address of the child
     */
    void sendSearchProbeReply(const MAC_ADDR& mac_addr);

    /**
     * Sends a should_connect request to a parent.
     * @param mac_addr the MAC address of the parent
     */
    void sendConnectRequest(const MAC_ADDR& mac_addr, SendPromise&& result_promise);

    /**
     * Sends a should_connect reply to a child.
     * @param mac_addr the MAC address of the child
     * @param accept whether the connection request is accepted
     */
    void sendConnectReply(const MAC_ADDR& mac_addr, bool accept, SendPromise&& result_promise);

    /**
     * Sends a child should_connect event to the root.
     * @param child_mac the MAC address of the child
     */
    void sendChildConnectEvent(const MAC_ADDR& child_mac, SendPromise&& result_promise);

    /**
     * Tries to should_connect to the best current parent.
     */
    void tryConnect();

    /**
     * Reject a possible parent.
     * @param mac_addr the MAC address of the parent
     */
    void rejectParent(MAC_ADDR mac_addr);

    std::shared_ptr<SendWorker> send_worker_;

    std::shared_ptr<NodeState> state_;

    std::shared_ptr<routing::Layout> layout_;

    std::shared_ptr<lwip::netif::Netif> netif_;

    bool searching_for_parents_{true};

    uint8_t min_channel_;
    uint8_t max_channel_;
    uint8_t current_channel_;

    std::vector<ParentInfo> parent_infos_{};

    /**
     * Ticks since the first parent was found while searching.
     */
    TickType_t first_parent_found_time_{0};

    /**
     * Ticks since the last should_connect request was sent.
     */
    TickType_t last_connect_request_time_{0};

    /**
     * Ticks since the last search probe was sent.
     */
    TickType_t last_search_probe_time_{0};
};

}  // namespace meshnow