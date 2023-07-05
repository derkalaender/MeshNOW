#pragma once

#include "espnow_multi.hpp"

namespace meshnow::receive {

class Receiver : espnow_multi::EspnowInterface {
   public:
    // doesn't do anything
    void sendCallback(const uint8_t* peer_addr, esp_now_send_status_t status) override{};

    // deserializes the item and if successful, puts it into the queue
    void receiveCallback(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) override;
};

}  // namespace meshnow::receive