#include "queue.hpp"

#include "util/queue.hpp"

static constexpr auto QUEUE_SIZE{10};

namespace meshnow::send {

static util::Queue<Item> queue;

esp_err_t init() { return queue.init(QUEUE_SIZE); }

void deinit() { queue = util::Queue<Item>{}; }

void enqueuePayload(const packets::Payload& payload, std::unique_ptr<SendBehavior> behavior, bool priority) {
    if (priority) {
        queue.push_front(Item{payload, std::move(behavior)}, portMAX_DELAY);
    } else {
        queue.push_back(Item{payload, std::move(behavior)}, portMAX_DELAY);
    }
}

std::optional<Item> popItem(TickType_t timeout) { return queue.pop(timeout); }

}  // namespace meshnow::send