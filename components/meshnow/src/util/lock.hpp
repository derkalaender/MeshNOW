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

    Lock(const Lock&) = delete;

    Lock& operator=(const Lock&) = delete;

    Lock(Lock&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

    Lock& operator=(Lock&& other) noexcept {
        if (this != &other) {
            xSemaphoreGive(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~Lock() {
        if (handle_ != nullptr) {
            xSemaphoreGive(handle_);
        }
    }

   private:
    SemaphoreHandle_t handle_;
};

}  // namespace meshnow::util