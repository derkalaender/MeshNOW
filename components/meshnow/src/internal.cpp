#include "internal.hpp"

#include <optional>

#include "util/mac.hpp"

namespace meshnow::internal {

static std::optional<util::MacAddr> this_mac{std::nullopt};
static bool root{false};

void setRoot(bool is_root) { root = is_root; }

bool isRoot() { return root; }

util::MacAddr getThisMac() {
    if (!this_mac.has_value()) {
        this_mac = util::MacAddr();
        esp_read_mac(this_mac->addr.data(), ESP_MAC_WIFI_STA);
    }
    return this_mac.value();
}

}  // namespace meshnow::internal