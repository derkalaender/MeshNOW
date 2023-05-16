#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "constants.hpp"

namespace meshnow::routing {

/**
 * Direct child node of the current node.
 * Contains the MAC adresses reachable by it in the routing_table.
 */
struct ChildNode {
    meshnow::MAC_ADDR mac_addr;
    uint8_t rssi;
    std::vector<meshnow::MAC_ADDR> routing_table;
};

/**
 * Direct parent node of the current node.
 */
struct ParentNode {
    meshnow::MAC_ADDR mac_addr;
    uint8_t rssi;
};

class RoutingInfo {
   public:
    /**
     * Constructor.
     * @param this_mac MAC address of the current node
     */
    explicit RoutingInfo(const meshnow::MAC_ADDR& this_mac);

    /**
     * Set the root node.
     * @param root_mac MAC address of the root node
     */
    void setRoot(const meshnow::MAC_ADDR& root_mac);

    /**
     * Set the parent node.
     * @param mac_addr MAC address of the parent node
     */
    void setParent(const meshnow::MAC_ADDR& mac_addr);

    /**
     * Add a direct child node.
     * @param mac_addr MAC address of the child node
     */
    void addChildNode(const meshnow::MAC_ADDR& mac_addr);

    /**
     * Add a node to the routing table of the given direct child node.
     * Does nothing if the given child MAC address is not a direct child.
     * @param child_mac MAC address of the direct child node
     * @param table_entry MAC address of the node to add to the routing table of the child node
     */
    void addToRoutingTable(const meshnow::MAC_ADDR& child_mac, const meshnow::MAC_ADDR& table_entry);

    /**
     * Update the RSSI of the node with the given MAC address.
     * Does nothing if the node is not a direct child or the parent.
     * @param mac_addr MAC address of the node to update
     * @param rssi RSSI of the node
     */
    void updateRssi(const meshnow::MAC_ADDR& mac_addr, uint8_t rssi);

    /**
     * Get the next hop to the given MAC address.
     * If the given MAC address is a direct child or in its routing table, its MAC address is returned.
     * Otherwise, the MAC address of the parent node is returned.
     * @param mac_addr MAC address of the destination node
     * @return MAC address of the next hop
     */
    meshnow::MAC_ADDR getNextHop(const meshnow::MAC_ADDR& mac_addr) const;

    /**
     * Get the MAC address of the root node.
     * @return MAC address of the root node
     */
    meshnow::MAC_ADDR getRootMac() const { return root_mac_.value(); }

    /**
     * Get the MAC address of the current node.
     * @return MAC address of the current node
     */
    meshnow::MAC_ADDR getThisMac() const { return this_mac_; }

   private:
    meshnow::MAC_ADDR this_mac_;
    std::optional<meshnow::MAC_ADDR> root_mac_;
    std::optional<ParentNode> parent_node_;
    std::vector<ChildNode> child_nodes_;
};

}  // namespace meshnow::routing
