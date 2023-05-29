#pragma once

#include <esp_mac.h>

#include <list>
#include <memory>
#include <optional>

#include "constants.hpp"

namespace meshnow::routing {

static MAC_ADDR queryThisMac() {
    meshnow::MAC_ADDR mac;
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    return mac;
}

template <typename T>
struct NodeTree {
    explicit NodeTree(const MAC_ADDR& mac) : mac{mac} {}

    MAC_ADDR mac;
    std::list<std::shared_ptr<T>> children;
};

struct IndirectChild : NodeTree<IndirectChild> {};

struct DirectChild : NodeTree<IndirectChild> {
    using NodeTree::NodeTree;

    int rssi{0};
};

struct Parent {
    explicit Parent(const MAC_ADDR& mac) : mac{mac} {}

    MAC_ADDR mac;
    int rssi{0};
};

struct Layout : NodeTree<DirectChild> {
    Layout() : NodeTree{queryThisMac()} {}

    std::optional<Parent> parent;
};

template <typename T>
bool contains(const T& tree, const MAC_ADDR& mac) {
    if (tree.mac == mac) {
        return true;
    }

    for (const auto& child : tree.children) {
        if (contains(*child, mac)) {
            return true;
        }
    }
    return false;
}

}  // namespace meshnow::routing