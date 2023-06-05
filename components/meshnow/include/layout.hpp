#pragma once

#include <esp_mac.h>
#include <freertos/portmacro.h>

#include <algorithm>
#include <list>
#include <memory>
#include <mutex>
#include <optional>

#include "constants.hpp"

namespace meshnow::routing {

static MAC_ADDR queryThisMac() {
    meshnow::MAC_ADDR mac;
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    return mac;
}

struct Node {
    explicit Node(const MAC_ADDR& mac) : mac(mac) {}

    MAC_ADDR mac;
};

struct Neighbor : Node {
    using Node::Node;

    int rssi{0};
    TickType_t last_seen{0};
};

template <typename T>
struct NodeTree {
    std::list<std::shared_ptr<T>> children;
};

struct IndirectChild : Node, NodeTree<IndirectChild> {
    using Node::Node;
};

struct DirectChild : Neighbor, NodeTree<IndirectChild> {
    using Neighbor::Neighbor;
};

struct Layout : Node, NodeTree<DirectChild> {
    Layout() : Node(queryThisMac()), NodeTree() {}

    std::shared_ptr<Neighbor> parent;
    std::optional<MAC_ADDR> root;

    std::mutex mtx;
};

// FUNCTIONS //

inline std::vector<std::shared_ptr<Neighbor>> getNeighbors(const std::shared_ptr<Layout>& layout) {
    assert(layout);
    std::vector<std::shared_ptr<Neighbor>> neighbors;
    if (layout->parent) {
        neighbors.push_back(layout->parent);
    }
    neighbors.insert(neighbors.end(), layout->children.begin(), layout->children.end());
    return neighbors;
}

inline bool containsDirectChild(const std::shared_ptr<Layout>& layout, const MAC_ADDR& mac) {
    assert(layout);
    return std::ranges::any_of(layout->children, [&mac](auto&& child) { return child->mac == mac; });
}

inline bool hasNeighbor(const std::shared_ptr<Layout>& layout, const MAC_ADDR& mac) {
    assert(layout);
    return (layout->parent && layout->parent->mac == mac) || containsDirectChild(layout, mac);
}

template <typename T>
inline bool containsChild(const std::shared_ptr<T>& tree, const MAC_ADDR& mac) {
    assert(tree);
    return std::ranges::any_of(tree->children,
                               [&mac](auto&& child) { return child->mac == mac || containsChild(child, mac); });
}

inline std::optional<MAC_ADDR> resolve(const std::shared_ptr<Layout>& layout, const MAC_ADDR& dest) {
    assert(layout);
    if (dest == layout->mac || dest == BROADCAST_MAC_ADDR) {
        // don't do anything
        return dest;
    } else if (dest == ROOT_MAC_ADDR) {
        // if we are the root, then return ourselves
        if (layout->mac == ROOT_MAC_ADDR) return layout->mac;
        // try to resolve the parent
        if (layout->parent) {
            return layout->parent->mac;
        } else {
            return std::nullopt;
        }
    } else if (layout->parent && dest == layout->parent->mac) {
        return layout->parent->mac;
    }

    // try to find a suitable child
    auto child = std::find_if(layout->children.begin(), layout->children.end(),
                              [&dest](auto&& child) { return containsChild(child, dest); });

    if (child != layout->children.end()) {
        // found the child
        return (*child)->mac;
    } else {
        // did not find the child, return the parent per default
        if (layout->parent) {
            return layout->parent->mac;
        } else {
            return std::nullopt;
        }
    }
}

// Wrappers for creating a child //

template <typename T>
inline auto createChild(const MAC_ADDR& mac);

template <>
inline auto createChild<Layout>(const MAC_ADDR& mac) {
    return std::make_shared<DirectChild>(mac);
}

template <>
inline auto createChild<DirectChild>(const MAC_ADDR& mac) {
    return std::make_shared<IndirectChild>(mac);
}

template <>
inline auto createChild<IndirectChild>(const MAC_ADDR& mac) {
    return std::make_shared<IndirectChild>(mac);
}

// Children handling functions //

template <typename T>
inline bool insertChild(const std::shared_ptr<T>& tree, const MAC_ADDR& parent_mac, const MAC_ADDR& child_mac) {
    assert(tree);
    if (tree->mac == parent_mac) {
        tree->children.emplace_back(createChild<T>(child_mac));
        return true;
    }

    for (auto&& child : tree->children) {
        if (insertChild(child, parent_mac, child_mac)) {
            return true;
        }
    }

    return false;
}

inline void insertDirectChild(const std::shared_ptr<Layout>& tree, DirectChild&& child) {
    assert(tree);
    tree->children.push_back(std::make_shared<DirectChild>(child));
}

inline bool removeDirectChild(const std::shared_ptr<Layout>& tree, const MAC_ADDR& child_mac) {
    assert(tree);
    auto child = std::find_if(tree->children.begin(), tree->children.end(),
                              [&child_mac](auto&& child) { return child->mac == child_mac; });

    if (child != tree->children.end()) {
        tree->children.erase(child);
        return true;
    } else {
        return false;
    }
}

}  // namespace meshnow::routing