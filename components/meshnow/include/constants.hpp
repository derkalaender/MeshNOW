#pragma once

#include <esp_now.h>

#include <array>
#include <cstdint>

#include "networking.hpp"

#define MAC_FORMAT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_FORMAT_ARGS(mac_addr) \
    (mac_addr)[0], (mac_addr)[1], (mac_addr)[2], (mac_addr)[3], (mac_addr)[4], (mac_addr)[5]

namespace meshnow {
const int MAC_ADDR_LEN{ESP_NOW_ETH_ALEN};
using MAC_ADDR = std::array<uint8_t, MAC_ADDR_LEN>;
const MAC_ADDR BROADCAST_MAC_ADDR{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const int MAX_RAW_PACKET_SIZE{ESP_NOW_MAX_DATA_LEN};
const int MAX_DATA_TOTAL_SIZE{1500};  // MTU of IPv4 by LwIP

const int MAX_FRAG_NUM{7};                        // 1500/250=6 + possible additional fragment because of header space
const int MAX_SEQ_NUM{8191};                      // 13 bits for the sequence number is 2^13 - 1 (should be enough)
const int DATA_HEADER_FIRST_SIZE{3 + 1 + 6 + 3};  // magic + type + target mac + seq&len
const int MAX_DATA_FIRST_SIZE{MAX_RAW_PACKET_SIZE - DATA_HEADER_FIRST_SIZE};
const int DATA_HEADER_NEXT_SIZE{DATA_HEADER_FIRST_SIZE - 1};  // only two bytes for seq&frag
const int MAX_DATA_NEXT_SIZE{MAX_RAW_PACKET_SIZE - DATA_HEADER_NEXT_SIZE};
}  // namespace meshnow