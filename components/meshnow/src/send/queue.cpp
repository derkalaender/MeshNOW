#include "queue.hpp"

#include "util/queue.hpp"

static constexpr auto QUEUE_SIZE{10};

namespace meshnow::send {

static util::Queue<Item> queue;

esp_err_t init() { return queue.init(QUEUE_SIZE); }

void deinit() { queue = util::Queue<Item>{}; }

void enqueuePacket(util::MacAddr dest_addr, packets::Packet packet, bool resolve, bool one_shot, bool priority) {
    if (priority) {
        queue.push_front(Item{dest_addr, packet, resolve, one_shot}, portMAX_DELAY);

    } else {
        queue.push_back(Item{dest_addr, packet, resolve, one_shot}, portMAX_DELAY);
    }
}

std::optional<Item> popPacket(TickType_t timeout) { return queue.pop(timeout); }

}  // namespace meshnow::send