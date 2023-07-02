#include "layout.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <memory>

namespace meshnow::routing {

static SemaphoreHandle_t mtx{nullptr};
static Layout layout;

esp_err_t init() {
    mtx = xSemaphoreCreateMutex();
    if (mtx == nullptr) return ESP_ERR_NO_MEM;

    return ESP_OK;
}

void deinit() {
    layout = Layout{};
    assert(mtx);
    vSemaphoreDelete(mtx);
    mtx = nullptr;
}

SemaphoreHandle_t getMtx() {
    assert(mtx);
    return mtx;
}

Layout& getLayout() { return layout; }

std::vector<std::shared_ptr<Neighbor>> getNeighbors(const std::shared_ptr<Layout>& layout) {
    assert(layout);
    std::vector<std::shared_ptr<Neighbor>> neighbors;
    if (layout->parent) {
        neighbors.push_back(layout->parent);
    }
    neighbors.insert(neighbors.end(), layout->children.begin(), layout->children.end());
    return neighbors;
}

bool containsDirectChild(const std::shared_ptr<Layout>& layout, const MAC_ADDR& mac) {
    assert(layout);
    return std::ranges::any_of(layout->children, [&mac](auto&& child) { return child->mac == mac; });
}

bool hasNeighbor(const std::shared_ptr<Layout>& layout, const MAC_ADDR& mac) {
    assert(layout);
    return (layout->parent && layout->parent->mac == mac) || containsDirectChild(layout, mac);
}

std::optional<MAC_ADDR> resolve(const std::shared_ptr<Layout>& layout, const MAC_ADDR& dest) {
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
                              [&dest](auto&& child) { return child->mac == dest || containsChild(child, dest); });

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

void insertDirectChild(const std::shared_ptr<Layout>& tree, DirectChild&& child) {
    assert(tree);
    tree->children.push_back(std::make_shared<DirectChild>(child));
}

bool removeDirectChild(const std::shared_ptr<Layout>& tree, const MAC_ADDR& child_mac) {
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