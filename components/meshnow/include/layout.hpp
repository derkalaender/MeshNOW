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

struct IndirectChild : NodeTree<IndirectChild> {
    using NodeTree::NodeTree;
};

struct DirectChild : NodeTree<IndirectChild> {
    using NodeTree::NodeTree;

    int rssi{0};
};

struct Parent {
    explicit Parent(const MAC_ADDR& mac) : mac{mac} {}

    MAC_ADDR mac;
    int rssi{0};
};

struct Layout : NodeTree<DirectChild>, std::enable_shared_from_this<Layout> {
    Layout() : NodeTree{queryThisMac()} {}

    std::optional<Parent> parent;
};

template <typename T>
bool contains(const std::shared_ptr<T>& tree, const MAC_ADDR& mac) {
    if (tree->mac == mac) {
        return true;
    }

    for (const auto& child : tree->children) {
        if (contains(child, mac)) {
            return true;
        }
    }
    return false;
}

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

template <typename T>
bool append(const std::shared_ptr<T>& tree, const MAC_ADDR& find_mac, const MAC_ADDR& append_mac) {
    if (tree->mac == find_mac) {
        tree->children.emplace_back(createChild<T>(append_mac));
        return true;
    }

    for (const auto& child : tree->children) {
        if (append(child, find_mac, append_mac)) {
            return true;
        }
    }

    return false;
}

template <typename T>
bool remove(std::shared_ptr<T> tree, const MAC_ADDR& mac) {
    if (tree->mac == mac) {
        return false;
    }

    for (const auto& child : tree->children) {
        if (child->mac == mac) {
            tree->children.remove(child);
            return true;
        }
    }

    for (const auto& child : tree->children) {
        if (remove(child, mac)) {
            return true;
        }
    }

    return false;
}

}  // namespace meshnow::routing