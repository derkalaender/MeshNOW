#pragma once

#include <cstdint>

namespace meshnow {

/**
 * Returns a new sequence number (+1 of the last)
 */
std::uint16_t generateSequenceNumber();

}  // namespace meshnow