#include "routing.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include "constants.hpp"

meshnow::routing::RoutingInfo::RoutingInfo(const meshnow::MAC_ADDR& this_mac)
    : this_mac_{this_mac}, root_mac_{std::nullopt}, parent_node_{std::nullopt}, child_nodes_{} {}

void meshnow::routing::RoutingInfo::setRoot(const meshnow::MAC_ADDR& root_mac) { root_mac_ = root_mac; }

void meshnow::routing::RoutingInfo::setParent(const meshnow::MAC_ADDR& mac_addr) {
    parent_node_ = ParentNode{mac_addr, 0};
}

void meshnow::routing::RoutingInfo::addChildNode(const meshnow::MAC_ADDR& mac_addr) {
    child_nodes_.push_back(ChildNode{mac_addr, 0, {}});
}

void meshnow::routing::RoutingInfo::addToRoutingTable(const meshnow::MAC_ADDR& child_mac,
                                                      const meshnow::MAC_ADDR& table_entry) {
    for (auto& child_node : child_nodes_) {
        if (child_node.mac_addr == child_mac) {
            child_node.routing_table.push_back(table_entry);
            return;
        }
    }
}

void meshnow::routing::RoutingInfo::updateRssi(const meshnow::MAC_ADDR& mac_addr, int8_t rssi) {
    if (parent_node_ && parent_node_->mac_addr == mac_addr) {
        parent_node_->rssi = rssi;
    } else {
        for (auto& child_node : child_nodes_) {
            if (child_node.mac_addr == mac_addr) {
                child_node.rssi = rssi;
                return;
            }
        }
    }
}

meshnow::MAC_ADDR meshnow::routing::RoutingInfo::getNextHop(const meshnow::MAC_ADDR& mac_addr) const {
    for (const auto& child_node : child_nodes_) {
        // if its the direct child, return its MAC
        if (child_node.mac_addr == mac_addr) {
            return mac_addr;
        }
        // otherwise check if the destination node is in its routing table
        for (const auto& entry : child_node.routing_table) {
            if (entry == mac_addr) {
                return child_node.mac_addr;
            }
        }
    }

    // else return the parent node
    return parent_node_->mac_addr;
}
