#include "io.hpp"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_random.h>

#include "state.hpp"
#include "util/util.hpp"

namespace meshnow::lwip::io {

static constexpr auto TAG = CREATE_TAG("LWIP | IO");

// STATIC IMPLS FOR DRIVER CONFIG

static esp_err_t transmit_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len) {
    return static_cast<meshnow::lwip::io::IODriver*>(driver_handle)->driver_impl->transmit(buffer, len);
}

static esp_err_t transmit_wrap_static(esp_netif_iodriver_handle driver_handle, void* buffer, size_t len,
                                      void* netstack_buffer) {
    // we simply call the non-wrap version. the normal wifi driver does this too. see <wifi_netif.c>/wifi_transmit_wrap
    return transmit_static(driver_handle, buffer, len);
}

static void driver_free_rx_buffer_static(esp_netif_iodriver_handle driver_handle, void* buffer) {
    // simply free
    if (buffer) {
        free(buffer);
    }
}

esp_err_t meshnow::lwip::io::postAttachCallback(esp_netif_t* esp_netif, esp_netif_iodriver_handle driver_handle) {
    ESP_LOGI(TAG, "Post attach callback called");
    auto driver = static_cast<meshnow::lwip::io::IODriver*>(driver_handle);
    driver->base.netif = esp_netif;
    esp_netif_driver_ifconfig_t driver_ifconfig = {.handle = driver,
                                                   .transmit = transmit_static,
                                                   .transmit_wrap = transmit_wrap_static,
                                                   .driver_free_rx_buffer = driver_free_rx_buffer_static};
    return esp_netif_set_driver_config(esp_netif, &driver_ifconfig);
}

using meshnow::lwip::io::IODriverImpl;

IODriverImpl::IODriverImpl(std::shared_ptr<esp_netif_t> netif) : netif_(std::move(netif)) {}

void meshnow::lwip::io::IODriverImpl::receivedData(const util::Buffer& buffer) {
    ESP_LOGD(TAG, "Received data via MeshNOW. Forwarding to Netif layer");
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer.data(), buffer.size(), ESP_LOG_VERBOSE);
    void* data_ptr = const_cast<void*>(reinterpret_cast<const void*>(buffer.data()));
    esp_netif_receive(netif_.get(), data_ptr, buffer.size(), nullptr);
}

void IODriverImpl::sendData(const util::MacAddr& mac, void* buffer, size_t len) {
    if (len > 1500) {  // TODO magic constant
        ESP_LOGE(TAG, "Data too large to send via MeshNOW");
        return;
    }

    ESP_LOGD(TAG, "Sending data of length %d", len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_VERBOSE);

    auto buf8 = static_cast<uint8_t*>(buffer);
    uint16_t remaining = len;
    uint32_t id{esp_random()};

    // TODO

    //    // send the first payload
    //    {
    //        ESP_LOGD(TAG, "Queueing payload 0");
    //
    //        uint16_t size{std::min(remaining, MAX_DATA_FIRST_SIZE)};
    //        Buffer payload_buf(size);
    //        // copy into payload
    //        std::copy(buf8, buf8 + size, payload_buf.begin());
    //
    //        // send the first fragment
    //        packets::LwipDataFirst payload{
    //            .source = layout_->mac, .target = mac, .id = id, .size = remaining, .data = std::move(payload_buf)};
    //        send_worker_->enqueuePayload(mac, true, payload, SendPromise{}, false, QoS::NEXT_HOP);
    //    }
    //
    //    if (len <= MAX_DATA_FIRST_SIZE) {
    //        // no need to send more fragments
    //        return;
    //    }
    //
    //    uint16_t offset = MAX_DATA_FIRST_SIZE;
    //    remaining -= MAX_DATA_FIRST_SIZE;
    //    uint8_t frag_num = 1;
    //
    //    // send the rest of the payloads
    //    while (remaining > 0) {
    //        ESP_LOGD(TAG, "Queueing payload %d", frag_num);
    //
    //        uint16_t size{std::min(remaining, MAX_DATA_NEXT_SIZE)};
    //        Buffer payload_buf(size);
    //        // copy into payload
    //        std::copy(buf8 + offset, buf8 + offset + size, payload_buf.begin());
    //
    //        // send the payload
    //        packets::LwipDataNext payload{
    //            .source = layout_->mac, .target = mac, .id = id, .frag_num = frag_num, .data =
    //            std::move(payload_buf)};
    //        send_worker_->enqueuePayload(mac, true, payload, SendPromise{}, false, QoS::NEXT_HOP);
    //
    //        offset += size;
    //        remaining -= size;
    //        frag_num++;
    //    }
}

using meshnow::lwip::io::RootIODriver;

esp_err_t RootIODriver::transmit(void* buffer, size_t len) {
    // TODO

    //    meshnow::MAC_ADDR dest_mac;
    //    std::copy(static_cast<uint8_t*>(buffer), static_cast<uint8_t*>(buffer) + 6, dest_mac.begin());
    //
    //    ESP_LOGD(TAG, "Transmitting packet to node: " MAC_FORMAT, MAC_FORMAT_ARGS(dest_mac));
    //
    //    if (dest_mac == meshnow::BROADCAST_MAC_ADDR) {
    //        // broadcast case
    //
    //        layout::forEachChild(layout_, [&](auto&& node) {
    //            auto child_mac = node->mac;
    //            ESP_LOGD(TAG, "Sending to child: " MAC_FORMAT, MAC_FORMAT_ARGS(child_mac));
    //            sendData(child_mac, buffer, len);
    //        });
    //    } else {
    //        // p2p case
    //        sendData(dest_mac, buffer, len);
    //    }
    //
    return ESP_OK;
}

using meshnow::lwip::io::NodeIODriver;

esp_err_t NodeIODriver::transmit(void* buffer, size_t len) {
    // TODO

    //    meshnow::MAC_ADDR dest_mac;
    //    std::copy(static_cast<uint8_t*>(buffer), static_cast<uint8_t*>(buffer) + 6, dest_mac.begin());
    //
    //    ESP_LOGD(TAG, "Transmitting packet to root, with dest addr: " MAC_FORMAT, MAC_FORMAT_ARGS(dest_mac));
    //
    //    // TODO maybe shortcut if the dest mac is a child or the parent, we can directly send to those without going
    //    // through the root
    //
    //    sendData(meshnow::ROOT_MAC_ADDR, buffer, len);
    //    sendData(*layout_->root, buffer, len);

    return ESP_OK;
}

}  // namespace meshnow::lwip::io