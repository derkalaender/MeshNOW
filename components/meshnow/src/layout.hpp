#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <optional>
#include <vector>

#include "state.hpp"
#include "util/mac.hpp"

namespace meshnow::routing {

struct Node {
    util::MacAddr mac;
};

struct Neighbor : Node {
    using Node::Node;

    TickType_t last_seen{0};
};

template <typename T>
struct NodeTree {
    std::vector<T> children;
};

struct IndirectChild : Node, NodeTree<IndirectChild> {
    using Node::Node;
};

struct DirectChild : Neighbor, NodeTree<IndirectChild> {
    using Neighbor::Neighbor;
};

struct Layout : Node, NodeTree<DirectChild> {
    Layout() : Node(state::getThisMac()), NodeTree() {}

    std::optional<Neighbor> parent;
};

esp_err_t init();
void deinit();

SemaphoreHandle_t getMtx();

Layout& getLayout();

// FUNCTIONS //

bool hasNeighbors();

bool contains(const util::MacAddr& mac);

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