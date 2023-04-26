#pragma once

#include <esp_now.h>

#include <array>
#include <cstdint>

#define MAC_FORMAT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_FORMAT_ARGS(mac_addr) \
    (mac_addr)[0], (mac_addr)[1], (mac_addr)[2], (mac_addr)[3], (mac_addr)[4], (mac_addr)[5]

namespace MeshNOW {
const int MAC_ADDR_LEN{ESP_NOW_ETH_ALEN};
using MAC_ADDR = std::array<uint8_t, MAC_ADDR_LEN>;
const MAC_ADDR BROADCAST_MAC_ADDR{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const int MAX_RAW_PAYLOAD_SIZE{ESP_NOW_MAX_DATA_LEN};
}  // namespace MeshNOW