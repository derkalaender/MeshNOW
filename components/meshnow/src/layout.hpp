#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <optional>
#include <vector>

#include "state.hpp"
#include "util/mac.hpp"

namespace meshnow::layout {

struct Node {
    explicit Node(const util::MacAddr& mac) : mac(mac) {}

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

bool hasNeighbor(const util::MacAddr& mac);

decltype(getLayout().children.begin()) getDirectChild(const util::MacAddr& mac);

bool hasDirectChild(const util::MacAddr& mac);

bool has(const util::MacAddr& mac);

void addDirectChild(const util::MacAddr& mac);

void addIndirectChild(const util::MacAddr& parent, const util::MacAddr& child);

void removeIndirectChild(const util::MacAddr& parent, const util::MacAddr& child);

}  // namespace meshnow::layout