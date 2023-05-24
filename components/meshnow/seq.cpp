#include "seq.hpp"

#include <atomic>

static std::atomic_uint16_t sequence_number{0};

std::uint16_t meshnow::generateSequenceNumber() { return sequence_number++; }