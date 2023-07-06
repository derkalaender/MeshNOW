#pragma once

#include "util/lock.hpp"

namespace meshnow {

// Global lock to synchonize State and Layout changes
util::Lock lock();

}  // namespace meshnow