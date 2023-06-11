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

constexpr uint16_t MAX_RAW_PACKET_SIZE{ESP_NOW_MAX_DATA_LEN};
constexpr uint16_t MAX_DATA_TOTAL_SIZE{1500};  // MTU of IPv4 by LwIP

constexpr uint16_t HEADER_SIZE{8};                         // magic + type + id
constexpr uint16_t DATA_FIRST_HEADER_SIZE{6 + 6 + 4 + 2};  // source mac + target mac + id + total size
constexpr uint16_t DATA_NEXT_HEADER_SIZE{6 + 6 + 4 + 1};   // source mac + target mac + id + fragment number
constexpr uint16_t MAX_DATA_FIRST_SIZE{MAX_RAW_PACKET_SIZE - HEADER_SIZE - DATA_FIRST_HEADER_SIZE};
constexpr uint16_t MAX_DATA_NEXT_SIZE{MAX_RAW_PACKET_SIZE - HEADER_SIZE - DATA_NEXT_HEADER_SIZE};

static inline consteval int calcMaxFragments() {
    constexpr int frags_for_data{MAX_DATA_TOTAL_SIZE / 250};
    constexpr int header_overhead{DATA_FIRST_HEADER_SIZE + (frags_for_data - 1) * DATA_NEXT_HEADER_SIZE};

    return frags_for_data + 1 + (header_overhead - 1) / MAX_DATA_NEXT_SIZE;
}

constexpr int MAX_FRAGMENTS{calcMaxFragments()};

constexpr int RECEIVE_QUEUE_SIZE{10};

using Buffer = std::vector<uint8_t>;
}  // namespace meshnow
