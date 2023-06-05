#pragma once

#include <freertos/portmacro.h>

#include <memory>
#include <optional>
#include <vector>

#include "constants.hpp"
#include "packet_handler.hpp"
#include "router.hpp"
#include "send_worker.hpp"
#include "state.hpp"
#include "waitbits.hpp"
#include "worker_task.hpp"

namespace meshnow {

/**
 * Whenever disconnected from a parent, this thread tries to connect to the best parent by continuously sending connect
 * requests.
 */
class HandShaker : public WorkerTask {
   public:
    explicit HandShaker(std::shared_ptr<SendWorker> send_worker, std::shared_ptr<NodeState> state,
                        std::shared_ptr<routing::Layout> layout);

    TickType_t nextActionAt() const noexcept override;

    void performAction() override;

    /**
     * Remove all found parents, prepare to start searching for parents again.
     */
    void reset();

    /**
     * @return the time at which the next action should be performed
     */
    TickType_t nextActionIn(TickType_t now) const;

    // Connection requesting methods //

    /**
     * Notify the Handshaker that a possible parent was found.
     * @param mac_addr the MAC address of the parent
     * @param rssi rssi of the parent
     */
    void foundPotentialParent(const MAC_ADDR& mac_addr, int rssi);

    /**
     * Notify the Handshaker that a parent responded to a connect request.
     * @param mac_addr the MAC address of the parent
     * @param accept whether the parent accepted the connect request
     */
    void receivedConnectResponse(const MAC_ADDR& mac_addr, bool accept, std::optional<MAC_ADDR> root_mac_addr);

    // Connection responding methods //

    /**
     * Handle a search probe from a child.
     * @param mac_addr MAC address of the child
     */
    void receivedSearchProbe(const MAC_ADDR& mac_addr);

    /**
     * Handle a connect request from a child.
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
     * Sends a connect request to a parent.
     * @param mac_addr the MAC address of the parent
     */
    void sendConnectRequest(const MAC_ADDR& mac_addr, SendPromise&& result_promise);

    /**
     * Sends a connect reply to a child.
     * @param mac_addr the MAC address of the child
     * @param accept whether the connection request is accepted
     */
    void sendConnectReply(const MAC_ADDR& mac_addr, bool accept, SendPromise&& result_promise);

    /**
     * Sends a child connect event to the root.
     * @param child_mac the MAC address of the child
     */
    void sendChildConnectEvent(const MAC_ADDR& child_mac, SendPromise&& result_promise);

    /**
     * Tries to connect to the best current parent.
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

    bool searching_for_parents_{true};

    std::vector<ParentInfo> parent_infos_{};

    /**
     * Ticks since the first parent was found while searching.
     */
    TickType_t first_parent_found_time_{0};

    /**
     * Ticks since the last connect request was sent.
     */
    TickType_t last_connect_request_time_{0};

    /**
     * Ticks since the last search probe was sent.
     */
    TickType_t last_search_probe_time_{0};
};

}  // namespace meshnow