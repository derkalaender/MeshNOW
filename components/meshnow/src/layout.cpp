#include "layout.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace meshnow::layout {

SemaphoreHandle_t mtx() {
    static auto mtx = [] {
        auto mtx = xSemaphoreCreateMutex();
        assert(mtx);
        return mtx;
    }();
    return mtx;
}

Layout& Layout::get() {
    static Layout layout;
    return layout;
}

void Layout::reset() {
    parent_.reset();
    children_.fill({});
    num_children = 0;
}

std::span<Child> Layout::getChildren() { return std::span{children_.begin(), num_children}; }

std::optional<Neighbor>& Layout::getParent() { return parent_; }

bool Layout::isEmpty() const { return !hasParent() && !hasChildren(); }

bool Layout::hasChild(const util::MacAddr& child_mac) const {
    return std::any_of(children_.begin(), children_.begin() + num_children,
                       [&child_mac](const Child& child) { return child.mac == child_mac; });
}

bool Layout::hasParent() const { return parent_.has_value(); }

bool Layout::hasChildren() const { return num_children > 0; }

bool Layout::hasParent(const util::MacAddr& parent_mac) const { return false; }

bool Layout::hasNeighbor(const util::MacAddr& neighbor_mac) const { return false; }

bool Layout::has(const util::MacAddr& mac) const { return false; }

// FUNCTIONS //

bool hasNeighbors() {
    const auto& layout = getLayout();
    return layout.parent_ || !layout.children_.isEmpty();
}

bool hasNeighbor(const util::MacAddr& mac) { return getLayout().parent_.has_value() || hasDirectChild(mac); }

decltype(getLayout().children_.begin()) getDirectChild(const util::MacAddr& mac) {
    auto& children = getLayout().children_;
    return std::find_if(children.begin(), children.end(),
                        [&mac](const DirectChild& child) { return child.root_mac == mac; });
}

bool hasDirectChild(const util::MacAddr& mac) { return getDirectChild(mac) != getLayout().children_.end(); }

static bool containsChild(const auto& tree, const util::MacAddr& mac) {
    if (tree.root_mac == mac) return true;
    return std::any_of(tree.children_.begin(), tree.children_.end(),
                       [&mac](const auto& child) { return containsChild(child, mac); });
}

bool has(const util::MacAddr& mac) {
    const auto& layout = getLayout();
    if (layout.parent_ && layout.parent_->root_mac == mac) return true;

    return std::any_of(layout.children_.begin(), layout.children_.end(),
                       [&mac](const auto& child) { return containsChild(child, mac); });
}

void addDirectChild(const util::MacAddr& mac) {
    auto& layout = getLayout();
    DirectChild child{mac};
    child.last_seen = xTaskGetTickCount();
    layout.children_.emplace_back(std::move(child));
}

static bool addIndirectChildImpl(auto& tree, const util::MacAddr& parent, const util::MacAddr& child) {
    if (tree.root_mac == parent) {
        IndirectChild indirect_child{child};
        tree.children_.emplace_back(std::move(indirect_child));
        return true;
    }
    for (auto& child_node : tree.children_) {
        if (addIndirectChildImpl(child_node, parent, child)) return true;
    }
    return false;
}

void addIndirectChild(const util::MacAddr& parent, const util::MacAddr& child) {
    for (auto& child_node : getLayout().children_) {
        if (addIndirectChildImpl(child_node, parent, child)) return;
    }
}

static bool removeIndirectChildImpl(auto& tree, const util::MacAddr& parent, const util::MacAddr& child) {
    if (tree.root_mac == parent) {
        for (auto it = tree.children_.begin(); it != tree.children_.end(); ++it) {
            if (it->root_mac == child) {
                tree.children_.erase(it);
                break;
            }
        }
        return true;
    }
    for (auto& child_node : tree.children_) {
        if (removeIndirectChildImpl(child_node, parent, child)) return true;
    }
    return false;
}

void removeIndirectChild(const util::MacAddr& parent, const util::MacAddr& child) {
    for (auto& child_node : getLayout().children_) {
        if (removeIndirectChildImpl(child_node, parent, child)) return;
    }
}

}  // namespace meshnow::layout