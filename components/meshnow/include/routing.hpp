#pragma once

#include <optional>
#include <vector>

#include "constants.hpp"

namespace meshnow::routing {

/**
 * Direct child node of the current node.
 * Contains the MAC adresses reachable by it in the routing_table.
 */
struct ChildNode {
    MAC_ADDR mac_addr;
    int8_t rssi;
    std::vector<MAC_ADDR> routing_table;
};

/**
 * Direct parent node of the current node.
 */
struct ParentNode {
    MAC_ADDR mac_addr;
    int8_t rssi;
};

class RoutingInfo {
   public:
    /**
     * Constructor.
     * @param this_mac MAC address of the current node
     */
    explicit RoutingInfo(const MAC_ADDR& this_mac);

    /**
     * Set the root node.
     * @param root_mac MAC address of the root node
     */
    void setRoot(const MAC_ADDR& root_mac);

    /**
     * Set the parent node.
     * @param mac_addr MAC address of the parent node
     */
    void setParent(const MAC_ADDR& mac_addr);

    /**
     * Add a direct child node.
     * @param mac_addr MAC address of the child node
     */
    void addChildNode(const MAC_ADDR& mac_addr);

    /**
     * Add a node to the routing table of the given direct child node.
     * Does nothing if the given child MAC address is not a direct child.
     * @param child_mac MAC address of the direct child node
     * @param table_entry MAC address of the node to add to the routing table of the child node
     */
    void addToRoutingTable(const MAC_ADDR& child_mac, const MAC_ADDR& table_entry);

    /**
     * Update the RSSI of the node with the given MAC address.
     * Does nothing if the node is not a direct child or the parent.
     * @param mac_addr MAC address of the node to update
     * @param rssi RSSI of the node
     */
    void updateRssi(const MAC_ADDR& mac_addr, int8_t rssi);

    /**
     * Get the next hop to the given MAC address.
     * If the given MAC address is a direct child or in its routing table, its MAC address is returned.
     * Otherwise, the MAC address of the parent node is returned.
     * @param mac_addr MAC address of the destination node
     * @return MAC address of the next hop
     */
    MAC_ADDR getNextHop(const MAC_ADDR& mac_addr) const;

    /**
     * Get the MAC address of the root node.
     * @return MAC address of the root node
     */
    MAC_ADDR getRootMac() const { return root_mac_.value(); }

    /**
     * Get the MAC address of the parent node.
     * @return MAC address of the parent node
     */
    MAC_ADDR getParentMac() const { return parent_node_->mac_addr; }

    /**
     * Get the MAC address of the current node.
     * @return MAC address of the current node
     */
    MAC_ADDR getThisMac() const { return this_mac_; }

   private:
    MAC_ADDR this_mac_;
    std::optional<MAC_ADDR> root_mac_;
    std::optional<ParentNode> parent_node_;
    std::vector<ChildNode> child_nodes_;
};

}  // namespace meshnow::routing
