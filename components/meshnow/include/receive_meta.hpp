#pragma once

#include <cstdint>
#include "constants.hpp"

namespace meshnow {

struct ReceiveMeta {
    MAC_ADDR src_addr;
    MAC_ADDR dest_addr;
    uint16_t seq_num;
    int rssi;
};

}
