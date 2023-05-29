#pragma once

#include <esp_now.h>

#include <array>
#include <cstdint>
#include <vector>

#define MAC_FORMAT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_FORMAT_ARGS(mac_addr) \
    (mac_addr)[0], (mac_addr)[1], (mac_addr)[2], (mac_addr)[3], (mac_addr)[4], (mac_addr)[5]

namespace meshnow {

constexpr std::array<uint8_t, 3> MAGIC{0x55, 0x77, 0x55};

constexpr int MAC_ADDR_LEN{ESP_NOW_ETH_ALEN};
using MAC_ADDR = std::array<uint8_t, MAC_ADDR_LEN>;
constexpr MAC_ADDR BROADCAST_MAC_ADDR{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr MAC_ADDR ROOT_MAC_ADDR{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr int MAX_RAW_PACKET_SIZE{ESP_NOW_MAX_DATA_LEN};
constexpr int MAX_DATA_TOTAL_SIZE{1500};  // MTU of IPv4 by LwIP

constexpr int MAX_FRAG_NUM{7};    // 1500/250=6 + possible additional fragment because of header space
constexpr int MAX_SEQ_NUM{8191};  // 13 bits for the sequence number is 2^13 - 1 (should be enough)
constexpr int DATA_HEADER_FIRST_SIZE{3 + 1 + 6 + 3};  // magic + type + target mac + seq&len
constexpr int MAX_DATA_FIRST_SIZE{MAX_RAW_PACKET_SIZE - DATA_HEADER_FIRST_SIZE};
constexpr int DATA_HEADER_NEXT_SIZE{DATA_HEADER_FIRST_SIZE - 1};  // only two bytes for seq&frag
constexpr int MAX_DATA_NEXT_SIZE{MAX_RAW_PACKET_SIZE - DATA_HEADER_NEXT_SIZE};

constexpr int RECEIVE_QUEUE_SIZE{10};

constexpr int HEADER_SIZE{8};

using Buffer = std::vector<uint8_t>;
}  // namespace meshnow
