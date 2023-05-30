#pragma once

#include <optional>
#include <vector>

#include "constants.hpp"
#include "layout.hpp"

namespace meshnow::routing {

// TODO MUTEX because we access it from send worker and main loop

class Router {
   public:
    enum class RemoveResult {
        NONE,
        PARENT,
        CHILD,
    };

    explicit Router(bool is_root) : is_root_{is_root} {}

    /**
     * Resolve the next hop for a given MAC address.
     * @param mac MAC address of the target node
     * @return next hop MAC address if possible, otherwise std::nullopt
     */
    std::optional<MAC_ADDR> resolve(const MAC_ADDR& mac) const;

    /**
     * Whether the neighbor exists
     * @param mac MAC address of the neighbor
     * @return true if the neighbor exists, false otherwise
     */
    bool hasNeighbor(const MAC_ADDR& mac) const;

    std::vector<MAC_ADDR> getChildMacs() const;

    /**
     * Set the MAC address of the root node.
     * @param mac MAC address of the root node
     */
    void setRootMac(const MAC_ADDR& mac) { root_mac_ = mac; }

    /**
     * Get the MAC address of the root node.
     * @return MAC address of the root node
     */
    std::optional<MAC_ADDR> getRootMac() const { return is_root_ ? layout_->mac : root_mac_; }

    /**
     * Set the MAC address of the parent node.
     * @param mac MAC address of the parent node
     */
    void setParentMac(const MAC_ADDR& mac) { layout_->parent.emplace(mac); }

    std::optional<MAC_ADDR> getParentMac() const {
        if (layout_->parent) {
            return layout_->parent->mac;
        } else {
            return std::nullopt;
        }
    }

    /**
     * Adds the given child as a direct child of the given parent.
     * @param child_mac mac of the (in)direct child
     * @param parent_mac parent mac, can be this node's mac
     */
    void addChild(const MAC_ADDR& child_mac, const MAC_ADDR& parent_mac);

    MAC_ADDR getThisMac() const { return layout_->mac; }

    /**
     * Removes the given (indirect) child from the layout.
     * @param mac mac of the child to remove
     * @param mac
     */
    void removeChild(const MAC_ADDR& mac);

    /**
     * Removes the given parent or child from the layout.
     * @param mac mac of the node to remove
     * @return which kind of node was removed
     */
    RemoveResult removeNeighbor(const MAC_ADDR& mac);

    /**
     * Update the RSSI value of a node.
     * @param mac MAC address of the node
     * @param rssi last RSSI value of the node
     */
    void updateRssi(const MAC_ADDR& mac, int rssi);

   private:
    bool is_root_;
    std::optional<MAC_ADDR> root_mac_;
    std::shared_ptr<Layout> layout_{std::make_shared<Layout>()};
};

}  // namespace meshnow::routing
