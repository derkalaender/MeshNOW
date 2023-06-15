#pragma once

#include <freertos/portmacro.h>

#include <cstdint>
#include <map>

#include "constants.hpp"
#include "now_lwip/netif.hpp"
#include "worker_task.hpp"

namespace meshnow::fragment {

class DataEntry {
   public:
    explicit DataEntry(uint16_t total_size);

    void insertFragment(uint8_t frag_num, const Buffer& data);

    bool isComplete() const noexcept { return fragment_mask == (1 << num_fragments) - 1; }

    Buffer&& getData() && noexcept { return std::move(data_); }

    TickType_t lastFragmentReceived() const noexcept { return last_fragment_received_; }

   private:
    Buffer data_;
    uint8_t fragment_mask{0};
    uint8_t num_fragments;
    TickType_t last_fragment_received_{0};
};

/**
 * Keeps track of fragments and reassembles them into a single buffer. Times out fragments after a while.
 */
class FragmentTask : public WorkerTask {
   public:
    explicit FragmentTask(std::shared_ptr<lwip::netif::Netif> netif);

    TickType_t nextActionAt() const noexcept override;
    void performAction() override;

    void newFragmentFirst(const MAC_ADDR& src_mac, uint16_t fragment_id, uint16_t total_size, const Buffer& data);
    void newFragmentNext(const MAC_ADDR& src_mac, uint16_t fragment_id, uint8_t frag_num, const Buffer& data);

   private:
    void dataCompleted(Buffer&& data);

    using DataKey = std::pair<MAC_ADDR, uint16_t>;  // mac + fragment id
    std::map<DataKey, DataEntry> data_entries_;

    std::shared_ptr<lwip::netif::Netif> netif_;
};

}  // namespace meshnow::fragment