#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace meshnow::util {

/**
 * A RAII wrapper for a FreeRTOS semaphore.
 */
class Lock {
   public:
    explicit Lock(SemaphoreHandle_t handle) : handle_(handle) { xSemaphoreTake(handle_, portMAX_DELAY); }

    ~Lock() { xSemaphoreGive(handle_); }

   private:
    SemaphoreHandle_t handle_{};
};

}  // namespace meshnow::util