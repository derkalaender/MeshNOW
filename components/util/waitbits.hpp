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

    /**
     * Wait for the given bits to be set.
     * @param bits the bits to wait for (bitwise OR)
     * @param clearOnExit clear the set bits when function returns normally (no timeout)
     * @param waitForAllBits if true waits for all bits to be set, otherwise waits for any bit to be set
     * @param ticksToWait how long to wait for the bits to be set
     * @return the bits that were set during the timeout period
     */
    EventBits_t waitFor(EventBits_t bits, bool clearOnExit, bool waitForAllBits, TickType_t ticksToWait) {
        return xEventGroupWaitBits(event_group.get(), bits, clearOnExit, waitForAllBits, ticksToWait);
    }

    /**
     * Set the given bits.
     * @param bits the bits to set (bitwise OR)
     * @return the bits that were set at the time that this call returns
     */
    EventBits_t setBits(EventBits_t bits) { return xEventGroupSetBits(event_group.get(), bits); }

    /**
     * Clear the given bits.
     * @param bits the bits to clear (bitwise OR)
     * @return the bits that were cleared before this call
     */
    EventBits_t clearBits(EventBits_t bits) { return xEventGroupClearBits(event_group.get(), bits); }

    /**
     * Get the current bits.
     * @return the current bits
     */
    EventBits_t getBits() { return xEventGroupGetBits(event_group.get()); }

   private:
    std::unique_ptr<EventGroupDef_t, void (*)(EventGroupDef_t*)> event_group;
};
}  // namespace util