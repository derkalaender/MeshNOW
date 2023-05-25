#pragma once

#include <cstdint>

#include "constants.hpp"

namespace meshnow {

struct ReceiveMeta {
    MAC_ADDR src_addr;
    MAC_ADDR dest_addr;
    int rssi;
    uint16_t seq_num;
};

}  // namespace meshnow
