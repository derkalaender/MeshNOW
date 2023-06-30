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

std::vector<std::shared_ptr<Neighbor>> getNeighbors(const std::shared_ptr<Layout>& layout);

bool containsDirectChild(const std::shared_ptr<Layout>& layout, const MAC_ADDR& mac);

bool hasNeighbor(const std::shared_ptr<Layout>& layout, const MAC_ADDR& mac);

template <typename T>
inline bool containsChild(const std::shared_ptr<T>& tree, const MAC_ADDR& mac) {
    assert(tree);
    return std::ranges::any_of(tree->children,
                               [&mac](auto&& child) { return child->mac == mac || containsChild(child, mac); });
}

template <typename T, typename Func>
inline void forEachChild(const std::shared_ptr<T>& tree, Func&& func) {
    assert(tree);
    std::ranges::for_each(tree->children, [&](auto&& child) {
        func(child);
        forEachChild(child, func);
    });
}

std::optional<MAC_ADDR> resolve(const std::shared_ptr<Layout>& layout, const MAC_ADDR& dest);

// Wrappers for creating a child //

template <typename T>
static inline auto createChild(const MAC_ADDR& mac);

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

void insertDirectChild(const std::shared_ptr<Layout>& tree, DirectChild&& child);

bool removeDirectChild(const std::shared_ptr<Layout>& tree, const MAC_ADDR& child_mac);

}  // namespace meshnow::routing