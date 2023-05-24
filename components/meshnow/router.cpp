#include "router.hpp"

#include <esp_log.h>

#include <algorithm>
#include <optional>

#include "constants.hpp"
#include "internal.hpp"
#include "layout.hpp"
#include "optional"

static const char* TAG = "Router";

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

void meshnow::routing::Router::updateRssi(const MAC_ADDR& mac, int rssi) {
    if (mac == layout_.mac) {
        // ignore packets from self
        return;
    }

    if (layout_.parent && mac == layout_.parent->mac) {
        // update parent RSSI
        layout_.parent->rssi = rssi;
    } else {
        // update child RSSI
        auto child = std::find_if(layout_.children.begin(), layout_.children.end(),
                                  [&mac](auto&& child) { return routing::contains(child, mac); });
        if (child != layout_.children.end()) {
            child->rssi = rssi;
        }
    }

    ESP_LOGI(TAG, "Updated RSSI of neighbor " MAC_FORMAT " to %d", MAC_FORMAT_ARGS(mac), rssi);
}