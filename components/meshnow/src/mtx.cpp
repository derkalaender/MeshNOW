#include "mtx.hpp"

#include <freertos/semphr.h>

#include "util/lock.hpp"

namespace meshnow {

util::Lock lock() {
    static auto mtx = [] {
        auto mtx = xSemaphoreCreateMutex();
        assert(mtx && "Failed to create global mutex!");
        return mtx;
    }();
    return util::Lock{mtx};
}

}  // namespace meshnow