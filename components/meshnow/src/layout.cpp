#include "layout.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace meshnow::layout {

Layout& Layout::get() {
    static Layout layout;
    return layout;
}

void Layout::reset() {
    parent_.reset();
    children_.fill({});
    num_children = 0;
}

bool Layout::isEmpty() const { return !parent_ && !hasChildren(); }

bool Layout::hasChildren() const { return num_children > 0; }

bool Layout::has(const util::MacAddr& mac) const {
    // check parent
    if (parent_ && parent_->mac == mac) return true;

    // check children
    for (const auto& child : children_) {
        if (child.mac == mac) return true;
        // check routing table of child
        for (const auto& entry : child.routing_table) {
            if (entry.mac == mac) return true;
        }
    }

    return false;
}

std::span<Child> Layout::getChildren() { return std::span{children_.begin(), num_children}; }

std::optional<Neighbor>& Layout::getParent() { return parent_; }

void Layout::addChild(const util::MacAddr& addr) {
    if (num_children == MAX_CHILDREN) return;

    auto& child = children_[num_children++];
    child.mac = addr;
    child.last_seen = xTaskGetTickCount();
}

void Layout::removeChild(const util::MacAddr& mac) {
    auto it = std::find_if(children_.begin(), children_.end(), [&mac](const auto& child) { return child.mac == mac; });
    if (it == children_.end()) return;

    // remove child
    *it = {};
    std::rotate(it, it + 1, children_.end());
    --num_children;
}
}  // namespace meshnow::layout