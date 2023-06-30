#pragma once

#include <esp_err.h>
#include <esp_netif.h>

#include <memory>

#include "constants.hpp"
#include "layout.hpp"
#include "send/worker.hpp"

namespace meshnow::lwip::io {

class IODriverImpl {
   public:
    explicit IODriverImpl(std::shared_ptr<esp_netif_t> netif, std::shared_ptr<SendWorker> send_worker,
                          std::shared_ptr<routing::Layout> layout);

    virtual ~IODriverImpl() = default;

    void receivedData(const Buffer& buffer);

    // Only the transmit function needs to be a member and is different for root and node.
    virtual esp_err_t transmit(void* buffer, size_t len) = 0;

   protected:
    /**
     * Enqueues the buffer in the send worker.
     */
    void sendData(const meshnow::MAC_ADDR& mac, void* buffer, size_t len);

    std::shared_ptr<routing::Layout> layout_;

   private:
    // Static wrapper functions for the IO callbacks
    // For all of these, driver_handle is *this.
    // TODO make these noexcept

    std::shared_ptr<SendWorker> send_worker_;
    std::shared_ptr<esp_netif_t> netif_;
};

class RootIODriver : public IODriverImpl {
   public:
    using IODriverImpl::IODriverImpl;

   private:
    esp_err_t transmit(void* buffer, size_t len) override;
};

class NodeIODriver : public IODriverImpl {
   public:
    using IODriverImpl::IODriverImpl;

   private:
    esp_err_t transmit(void* buffer, size_t len) override;
};

/**
 * Post attach callback for netif. Will configure IO function callbacks.
 */
esp_err_t postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle);

struct IODriver {
    esp_netif_driver_base_t base{.post_attach = &postAttachCallback, .netif = nullptr};
    std::unique_ptr<IODriverImpl> driver_impl;
};

}  // namespace meshnow::lwip::io