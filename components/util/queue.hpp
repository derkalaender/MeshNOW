#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <memory>
#include <optional>

namespace util {

/**
 * Wraps a FreeRTOS thread-safe queue.
 * Lord forgive me for what I have done...
 */
template <typename T>
class Queue {
   public:
    explicit Queue(int max_items) : queue{xQueueCreate(max_items, sizeof(T)), vQueueDelete} {
        if (!queue) {
            throw std::runtime_error{"Failed to create receive_queue"};
        }
    }

    Queue(const Queue&) = delete;

    Queue& operator=(const Queue&) = delete;

    Queue(Queue&& other) noexcept : queue{std::move(other.queue)} {}

    Queue& operator=(Queue&& other) noexcept {
        queue = std::move(other.queue);
        return *this;
    }

    bool push_back(T&& item, TickType_t ticksToWait) {
        alignas(T) uint8_t buffer[sizeof(T)];
        new (buffer) T{std::move(item)};
        return xQueueSendToBack(queue.get(), static_cast<const void*>(buffer), ticksToWait);
    }

    bool push_front(T&& item, TickType_t ticksToWait) {
        alignas(T) uint8_t buffer[sizeof(T)];
        new (buffer) T{std::move(item)};
        return xQueueSendToFront(queue.get(), static_cast<const void*>(buffer), ticksToWait);
    }

    std::optional<T> pop(TickType_t ticksToWait) {
        alignas(T) uint8_t buffer[sizeof(T)];
        if (xQueueReceive(queue.get(), static_cast<void*>(buffer), ticksToWait)) {
            return std::move(*reinterpret_cast<T*>(buffer));
        } else {
            return std::nullopt;
        }
    }

    void clear() { xQueueReset(queue.get()); }

   private:
    std::unique_ptr<QueueDefinition, void (*)(QueueDefinition*)> queue;
};

}  // namespace util
