#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <optional>
#include <span>
#include <vector>

#include "state.hpp"
#include "util/mac.hpp"

namespace meshnow::layout {

// TODO
constexpr auto MAX_CHILDREN = 5;

struct Node {
    Node() = default;
    explicit Node(const util::MacAddr& mac) : mac{mac} {}
    util::MacAddr mac;
};

struct Neighbor : Node {
    using Node::Node;
    TickType_t last_seen{xTaskGetTickCount()};
};

struct Child : Neighbor {
    using Neighbor::Neighbor;
    std::vector<Node> routing_table;
};

struct Layout {
   public:
    static Layout& get();

    // delete copy and move
    Layout(const Layout&) = delete;
    Layout(Layout&&) = delete;
    Layout& operator=(const Layout&) = delete;
    Layout& operator=(Layout&&) = delete;

    /**
     * Resets the parent, all children, and their respective routing tables.
     */
    void reset();

    /**
     * Returns true iff there is no parent AND no children.
     */
    bool isEmpty() const;

    bool hasChildren() const;

    /**
     * Returns true iff the parent has the given mac OR a there is a child with the given mac OR the given mac is in the
     * routing table of a child.
     */
    bool has(const util::MacAddr& mac) const;

    std::optional<Neighbor>& getParent();

    std::span<Child> getChildren();

    void addChild(const util::MacAddr& addr);

    void removeChild(const util::MacAddr& mac);

   private:
    Layout() = default;
    ~Layout() = default;

    std::optional<Neighbor> parent_;
    std::array<Child, MAX_CHILDREN> children_;
    size_t num_children{0};
};

}  // namespace meshnow::layout