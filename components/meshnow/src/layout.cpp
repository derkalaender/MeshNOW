#include "layout.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace meshnow::routing {

static SemaphoreHandle_t mtx{nullptr};

esp_err_t init() {
    mtx = xSemaphoreCreateMutex();
    if (mtx == nullptr) return ESP_ERR_NO_MEM;

    return ESP_OK;
}

void deinit() {
    assert(mtx);
    vSemaphoreDelete(mtx);
    mtx = nullptr;
}

SemaphoreHandle_t getMtx() {
    assert(mtx);
    return mtx;
}

Layout& getLayout() {
    static Layout layout;
    return layout;
}

// FUNCTIONS //

bool hasNeighbors() {
    const auto& layout = getLayout();
    return layout.parent || !layout.children.empty();
}

bool hasNeighbor(const util::MacAddr& mac) { return getLayout().parent.has_value() || hasDirectChild(mac); }

decltype(getLayout().children.begin()) getDirectChild(const util::MacAddr& mac) {
    auto& children = getLayout().children;
    return std::find_if(children.begin(), children.end(),
                        [&mac](const DirectChild& child) { return child.mac == mac; });
}

bool hasDirectChild(const util::MacAddr& mac) { return getDirectChild(mac) != getLayout().children.end(); }

static bool containsChild(const auto& tree, const util::MacAddr& mac) {
    if (tree.mac == mac) return true;
    return std::any_of(tree.children.begin(), tree.children.end(),
                       [&mac](const auto& child) { return containsChild(child, mac); });
}

bool has(const util::MacAddr& mac) {
    const auto& layout = getLayout();
    if (layout.parent && layout.parent->mac == mac) return true;

    return std::any_of(layout.children.begin(), layout.children.end(),
                       [&mac](const auto& child) { return containsChild(child, mac); });
}

void addDirectChild(const util::MacAddr& mac) {
    auto& layout = getLayout();
    DirectChild child{mac};
    child.last_seen = xTaskGetTickCount();
    layout.children.emplace_back(std::move(child));
}

static bool addIndirectChildImpl(auto& tree, const util::MacAddr& parent, const util::MacAddr& child) {
    if (tree.mac == parent) {
        IndirectChild indirect_child{child};
        tree.children.emplace_back(std::move(indirect_child));
        return true;
    }
    for (auto& child_node : tree.children) {
        if (addIndirectChildImpl(child_node, parent, child)) return true;
    }
    return false;
}

void addIndirectChild(const util::MacAddr& parent, const util::MacAddr& child) {
    for (auto& child_node : getLayout().children) {
        if (addIndirectChildImpl(child_node, parent, child)) return;
    }
}

static bool removeIndirectChildImpl(auto& tree, const util::MacAddr& parent, const util::MacAddr& child) {
    if (tree.mac == parent) {
        for (auto it = tree.children.begin(); it != tree.children.end(); ++it) {
            if (it->mac == child) {
                tree.children.erase(it);
                break;
            }
        }
        return true;
    }
    for (auto& child_node : tree.children) {
        if (removeIndirectChildImpl(child_node, parent, child)) return true;
    }
    return false;
}

void removeIndirectChild(const util::MacAddr& parent, const util::MacAddr& child) {
    for (auto& child_node : getLayout().children) {
        if (removeIndirectChildImpl(child_node, parent, child)) return;
    }
}

}  // namespace meshnow::routing