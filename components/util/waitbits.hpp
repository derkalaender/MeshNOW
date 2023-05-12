#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <memory>

namespace util {

/**
 * Wraps a FreeRTOS event group.
 * Can be used for wait bits.
 */
class WaitBits {
   public:
    explicit WaitBits() : event_group{xEventGroupCreate(), vEventGroupDelete} {
        if (!event_group) {
            throw std::runtime_error{"Failed to create event group"};
        }
    }

    WaitBits(const WaitBits&) = delete;

    WaitBits& operator=(const WaitBits&) = delete;

    WaitBits(WaitBits&& other) noexcept : event_group{std::move(other.event_group)} {}

    WaitBits& operator=(WaitBits&& other) noexcept {
        event_group = std::move(other.event_group);
        return *this;
    }

    EventBits_t waitFor(EventBits_t bits, bool clearOnExit, bool waitForAllBits, TickType_t ticksToWait) {
        return xEventGroupWaitBits(event_group.get(), bits, clearOnExit, waitForAllBits, ticksToWait);
    }

    EventBits_t setBits(EventBits_t bits) { return xEventGroupSetBits(event_group.get(), bits); }

    EventBits_t clearBits(EventBits_t bits) { return xEventGroupClearBits(event_group.get(), bits); }

    EventBits_t getBits() { return xEventGroupGetBits(event_group.get()); }

   private:
    std::unique_ptr<EventGroupDef_t, void (*)(EventGroupDef_t*)> event_group;
};
}  // namespace util