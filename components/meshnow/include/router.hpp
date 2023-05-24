#pragma once

#include <optional>
#include <vector>

#include "constants.hpp"
#include "layout.hpp"

namespace meshnow::routing {

class Router {
   public:
    struct HopResult {
        bool reached_target_;
        std::optional<MAC_ADDR> next_hop_;
    };

    explicit Router(bool is_root) : is_root_{is_root} {}

    HopResult hopToNode(const MAC_ADDR& mac) const;

    HopResult hopToRoot() const;

    HopResult hopToParent() const;

    std::vector<MAC_ADDR> hopToChildren() const;

    /**
     * Set the MAC address of the root node.
     * @param mac MAC address of the root node
     */
    void setRootMac(const MAC_ADDR& mac) { root_mac_ = mac; }

    /**
     * Get the MAC address of the root node.
     * @return MAC address of the root node
     */
    std::optional<MAC_ADDR> getRootMac() const { return is_root_ ? layout_.mac : root_mac_; }

    /**
     * Set the MAC address of the parent node.
     * @param mac MAC address of the parent node
     */
    void setParentMac(const MAC_ADDR& mac) { layout_.parent.emplace(mac); }

   private:
    bool is_root_;
    std::optional<MAC_ADDR> root_mac_{};
    Layout layout_{};
};

}  // namespace meshnow::routing
