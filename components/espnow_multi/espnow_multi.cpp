#include "espnow_multi.hpp"

#include <esp_err.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <memory>

namespace espnow_multi {

namespace {

/**
 * Spinlock for accessing the singleton instance.
 */
portMUX_TYPE spin_ = portMUX_INITIALIZER_UNLOCKED;

void registerPeer(const uint8_t* peer_addr, uint8_t channel, wifi_interface_t ifidx) {
    esp_now_peer_info_t peer_info;
    peer_info.channel = channel;
    peer_info.ifidx = ifidx;
    std::copy(peer_addr, peer_addr + ESP_NOW_ETH_ALEN, peer_info.peer_addr);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

void unregisterPeer(const uint8_t* peer_addr) { ESP_ERROR_CHECK(esp_now_del_peer(peer_addr)); }

}  // namespace

class EspnowInterface::EspnowMulti {
   public:
    static std::shared_ptr<EspnowMulti> getInstance() {
        // current instance stored in a weak_ptr so that there is at least 0 and at most 1 instance
        static std::weak_ptr<EspnowMulti> instance;

        taskENTER_CRITICAL(&spin_);
        auto shared = instance.lock();
        if (!shared) {
            shared = std::make_shared<EspnowMulti>();
            instance = shared;
        }
        taskEXIT_CRITICAL(&spin_);

        return shared;
    }

    /**
     * ESP-NOW receive callback.
     */
    static void recv_cb(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
        auto multi = EspnowMulti::getInstance();
        // invoke the receive callback for all registered interfaces
        for (auto& weak : multi->interfaces_) {
            if (auto interface = weak.lock()) {
                interface->receiveCallback(esp_now_info, data, data_len);
            }
        }
    }

    /**
     * ESP-NOW send callback.
     */
    static void send_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {
        auto multi = EspnowMulti::getInstance();
        // invoke send_cb of the interface that sent the data
        if (auto interface = multi->last_interface_.lock()) {
            interface->sendCallback(mac_addr, status);
        }
        // can unregister the peer now so that there is space available again
        unregisterPeer(mac_addr);
        // free mutex so that the next send process can be started
        xSemaphoreGive(multi->send_mutex_);
    }

    /**
     * Send data to a peer. The given interface will be called when the data was sent.
     */
    esp_err_t send(std::shared_ptr<EspnowInterface> interface, const uint8_t* peer_addr, const uint8_t* data,
                   size_t len, uint8_t channel, wifi_interface_t ifidx) {
        // we want to wait indefinitely until the last data finished sending
        xSemaphoreTake(send_mutex_, portMAX_DELAY);
        last_interface_ = interface;
        // register peer before sending
        registerPeer(peer_addr, channel, ifidx);
        esp_err_t ret = esp_now_send(peer_addr, data, len);
        if (ret != ESP_OK) {
            // sending failed, send_cb will not be called -> release mutex here
            xSemaphoreGive(send_mutex_);
        }
        return ret;
    }

    EspnowMulti() {
        ESP_ERROR_CHECK(esp_now_init());
        ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
        ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
    }

    ~EspnowMulti() {
        ESP_ERROR_CHECK(esp_now_unregister_send_cb());
        ESP_ERROR_CHECK(esp_now_unregister_recv_cb());
        ESP_ERROR_CHECK(esp_now_deinit());
    }

    EspnowMulti(const EspnowMulti&) = delete;
    EspnowMulti& operator=(const EspnowMulti&) = delete;

    void registerInterface(const std::shared_ptr<EspnowInterface>& interface) {
        taskENTER_CRITICAL(&spin_);
        interfaces_.push_back(interface);
        taskEXIT_CRITICAL(&spin_);
    }

    void unregisterInterface(const std::shared_ptr<EspnowInterface>& interface) {
        taskENTER_CRITICAL(&spin_);
        std::erase_if(interfaces_, [interface](const std::weak_ptr<EspnowInterface>& ptr) {
            return ptr.expired() || ptr.lock() == interface;
        });
        taskEXIT_CRITICAL(&spin_);
    }

    // mutex to make other threads block until the last data finished sending
    SemaphoreHandle_t send_mutex_ = xSemaphoreCreateMutex();

    // the last interface used to send data
    // needed to call correct sendCallback
    // weak_ptr as it may have already been deleted when the send_cb is called
    std::weak_ptr<EspnowInterface> last_interface_;

    // all the registered interface
    std::vector<std::weak_ptr<EspnowInterface>> interfaces_;
};

EspnowInterface::EspnowInterface() { EspnowMulti::getInstance()->registerInterface(shared_from_this()); }

EspnowInterface::~EspnowInterface() { EspnowMulti::getInstance()->unregisterInterface(shared_from_this()); }

esp_err_t EspnowInterface::send(const uint8_t* peer_addr, const uint8_t* data, size_t len, uint8_t channel,
                                wifi_interface_t ifidx) {
    return EspnowMulti::getInstance()->send(shared_from_this(), peer_addr, data, len, channel, ifidx);
}

}  // namespace espnow_multi
