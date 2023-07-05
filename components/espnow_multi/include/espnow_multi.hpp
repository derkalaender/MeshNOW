#pragma once

#include <esp_now.h>

#include <memory>
#include <vector>

namespace espnow_multi {

class EspnowInterface : protected std::enable_shared_from_this<EspnowInterface> {
   public:
    EspnowInterface(const EspnowInterface&) = delete;
    EspnowInterface& operator=(const EspnowInterface&) = delete;
    virtual ~EspnowInterface();

    esp_err_t send(const uint8_t* peer_addr, const uint8_t* data, size_t len, uint8_t channel = 0,
                   wifi_interface_t ifidx = WIFI_IF_STA);

    virtual void sendCallback(const uint8_t* peer_addr, esp_now_send_status_t status) = 0;

    virtual void receiveCallback(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) = 0;

   protected:
    EspnowInterface();

   private:
    class EspnowMulti;
    std::shared_ptr<EspnowMulti> multi_instance_;
};

}  // namespace espnow_multi