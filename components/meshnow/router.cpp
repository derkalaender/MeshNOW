#include "router.hpp"

#include <algorithm>
#include <optional>

#include "constants.hpp"
#include "layout.hpp"
#include "optional"

meshnow::routing::Router::HopResult meshnow::routing::Router::hopToNode(const meshnow::MAC_ADDR& mac) const {
    if (mac == layout_.mac) {
        return HopResult{true, std::nullopt};
    } else if (layout_.parent && mac == layout_.parent->mac) {
        return HopResult{false, layout_.parent->mac};
    }

    // try to find a suitable child
    auto child = std::find_if(layout_.children.begin(), layout_.children.end(),
                              [&mac](auto&& child) { return routing::contains(child, mac); });

    if (child != layout_.children.end()) {
        // found the child
        return HopResult{false, child->mac};
    } else {
        // did not find the child, return the parent per default
        return HopResult{false, layout_.parent->mac};
    }
}

meshnow::routing::Router::HopResult meshnow::routing::Router::hopToRoot() const {
    if (is_root_) {
        return HopResult{true, std::nullopt};
    } else {
        return HopResult{false, root_mac_};
    }
}

meshnow::routing::Router::HopResult meshnow::routing::Router::hopToParent() const {
    if (is_root_) {
        return HopResult{true, std::nullopt};
    } else if (!layout_.parent) {
        return HopResult{false, std::nullopt};
    } else {
        return HopResult{false, layout_.parent->mac};
    }
}

std::vector<meshnow::MAC_ADDR> meshnow::routing::Router::hopToChildren() const {
    std::vector<MAC_ADDR> children;
    std::transform(layout_.children.begin(), layout_.children.end(), std::back_inserter(children),
                   [](const auto& child) { return child.mac; });
    return children;
}