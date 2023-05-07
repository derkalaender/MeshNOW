#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <memory>

namespace util {

/**
 * Wraps a FreeRTOS thread-safe queue.
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

    bool push_back(T& item, TickType_t ticksToWait) {
        return xQueueSendToBack(queue.get(), static_cast<const void*>(&item), ticksToWait);
    }

    bool push_front(const T& item, TickType_t ticksToWait) {
        return xQueueSendToFront(queue.get(), static_cast<const void*>(&item), ticksToWait);
    }
    std::optional<T> pop(TickType_t ticksToWait) {
        T item;
        if (xQueueReceive(queue.get(), static_cast<void*>(&item), ticksToWait)) {
            return item;
        } else {
            return std::nullopt;
        }
    }

   private:
    std::unique_ptr<QueueDefinition, void (*)(QueueDefinition*)> queue;
};

}  // namespace util
