#include "router.hpp"

#include <esp_log.h>

#include <optional>

#include "constants.hpp"
#include "internal.hpp"
#include "layout.hpp"

static const char* TAG = CREATE_TAG("Router");

std::optional<meshnow::MAC_ADDR> meshnow::routing::Router::resolve(const MAC_ADDR& mac) const {
    if (mac == layout_.mac || mac == BROADCAST_MAC_ADDR) {
        // don't do anything
        return mac;
    } else if (mac == ROOT_MAC_ADDR) {
        // if we are the root, then return ourselves
        if (is_root_) return layout_.mac;
        // try to resolve the parent
        if (layout_.parent) {
            return layout_.parent->mac;
        } else {
            return std::nullopt;
        }
    } else if (layout_.parent && mac == layout_.parent->mac) {
        return layout_.parent->mac;
    }

    // try to find a suitable child
    auto child = std::find_if(layout_.children.begin(), layout_.children.end(),
                              [&mac](auto&& child) { return routing::contains(*child, mac); });

    if (child != layout_.children.end()) {
        // found the child
        return (*child)->mac;
    } else {
        // did not find the child, return the parent per default
        if (layout_.parent) {
            return layout_.parent->mac;
        } else {
            return std::nullopt;
        }
    }
}

bool meshnow::routing::Router::hasNeighbor(const meshnow::MAC_ADDR& mac) const {
    if (mac == layout_.parent->mac) {
        return true;
    } else {
        return std::any_of(layout_.children.begin(), layout_.children.end(),
                           [&mac](auto& child) { return child->mac == mac; });
    }
}

std::vector<meshnow::MAC_ADDR> meshnow::routing::Router::getChildMacs() const {
    std::vector<MAC_ADDR> children;
    std::transform(layout_.children.begin(), layout_.children.end(), std::back_inserter(children),
                   [](const auto& child) { return child->mac; });
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
                                  [&mac](auto&& child) { return routing::contains(*child, mac); });
        if (child != layout_.children.end()) {
            (*child)->rssi = rssi;
        }
    }

    ESP_LOGV(TAG, "Updated RSSI of neighbor " MAC_FORMAT " to %d", MAC_FORMAT_ARGS(mac), rssi);
}

void meshnow::routing::Router::addChild(const MAC_ADDR& child_mac, const MAC_ADDR& parent_mac) {}