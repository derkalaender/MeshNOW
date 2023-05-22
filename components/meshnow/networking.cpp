#include "networking.hpp"

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>

#include <cstdint>
#include <variant>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "internal.hpp"
#include "packets.hpp"
#include "receive_meta.hpp"

static const char* TAG = CREATE_TAG("Networking");

meshnow::MAC_ADDR meshnow::Networking::queryThisMac() {
    meshnow::MAC_ADDR mac;
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    return mac;
}

// TODO handle list of peers full
static void add_peer(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGV(TAG, "Adding peer " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    if (esp_now_is_peer_exist(mac_addr.data())) {
        return;
    }
    esp_now_peer_info_t peer_info{};
    peer_info.channel = 0;
    peer_info.encrypt = false;
    peer_info.ifidx = WIFI_IF_STA;
    std::copy(mac_addr.begin(), mac_addr.end(), peer_info.peer_addr);
    CHECK_THROW(esp_now_add_peer(&peer_info));
}

void meshnow::Networking::start() {
    if (!state_.isRoot()) {
        ESP_LOGI(TAG, "Starting Handshaker");
        handshaker_.readyToConnect();
    } else {
        // update the routing info. Add our own MAC as the root MAC
        routing_info_.setRoot(routing_info_.getThisMac());
    }
}

void meshnow::Networking::rawSend(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data) {
    if (data.size() > MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum data size %d", data.size(), MAX_RAW_PACKET_SIZE);
        throw PayloadTooLargeException();
    }

    // TODO delete unused peers first
    add_peer(mac_addr);
    ESP_LOGV(TAG, "Sending raw data to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), data.data(), data.size()));
}

void meshnow::Networking::onSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // TODO
    ESP_LOGD(TAG, "Send status: %d", status);
    // notify send worker
    send_worker_.sendFinished(status == ESP_NOW_SEND_SUCCESS);
}

void meshnow::Networking::onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    ESP_LOGV(TAG, "Received data");
    // TODO error checking and timeout

    ReceiveMeta meta{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), meta.src_addr.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), meta.dest_addr.begin());
    meta.rssi = esp_now_info->rx_ctrl->rssi;

    std::vector<uint8_t> buffer(data, data + data_len);
    auto packet = meshnow::packets::deserialize(buffer);
    if (!packet) {
        // received invalid payload
        return;
    }

    // add sequence number to meta
    meta.seq_num = packet->seq_num;

    // call the corresponding handle function
    std::visit([&meta, this](auto&& p) { handle(meta, p); }, packet->payload);
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::StillAlive& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::AnyoneThere& p) {
    // TODO check if cannot accept any more children

    // only offer connection if we have a parent and can reach the root -> disconnected islands won't grow
    if (!state_.isRootReachable()) return;

    ESP_LOGI(TAG, "Sending I am here");
    send_worker_.enqueuePacket(meta.src_addr, meshnow::packets::Packet{0, meshnow::packets::IAmHere{}});
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::IAmHere& p) {
    // ignore when already connected (this packet came in late, we already chose a parent)
    if (state_.isConnected()) return;
    handshaker_.foundParent(meta.src_addr, meta.rssi);
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::PlsConnect& p) {
    // only accept if we can reach the root
    // this should have been handled by not sending the handleAnyoneThere packet, but race conditions and delays are a
    // thing
    if (!state_.isRootReachable()) return;

    // TODO need some reservation/synchronization mechanism so we don not allocate the same "child slot" to multiple
    // nodes
    // TODO add child information
    ESP_LOGI(TAG, "Sending welcome");
    // TODO check can accept
    send_worker_.enqueuePacket(meta.src_addr, packets::Packet{0, packets::Verdict{routing_info_.getRootMac(), true}});

    // send a node connected event to root
    send_worker_.enqueuePacket(routing_info_.getParentMac(), packets::Packet{0, packets::NodeConnected{meta.src_addr}});
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::Verdict& p) {
    // ignore if root or already connected (should actually never happen)
    if (state_.isRoot() || state_.isConnected()) return;

    if (p.accept) {
        ESP_LOGI(TAG, "Got accepted by parent: " MAC_FORMAT, MAC_FORMAT_ARGS(meta.src_addr));
        // we are safely connected and can stop searching for new parents now
        handshaker_.stopConnecting();
        state_.setConnected();
        // we assume we can reach the root because the parent only answers if it itself can reach the root
        state_.setRootReachable();
        // set the root MAC
        routing_info_.setRoot(p.root_mac);
        // set parent MAC
        routing_info_.setParent(meta.src_addr);
    } else {
        ESP_LOGI(TAG, "Got rejected by parent: " MAC_FORMAT, MAC_FORMAT_ARGS(meta.src_addr));
        // remove the possible parent and try connecting to other ones again
        handshaker_.reject(meta.src_addr);
        handshaker_.readyToConnect();
    }
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::NodeConnected& p) {
    // add to routing table
    routing_info_.addToRoutingTable(meta.src_addr, p.child_mac);
    // forward to parent, if not root
    if (!state_.isRoot()) {
        send_worker_.enqueuePacket(routing_info_.getParentMac(), packets::Packet{0, p});
    }
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::NodeDisconnected& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::MeshUnreachable& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::MeshReachable& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::Ack& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::Nack& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::LwipDataFirst& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::CustomDataFirst& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::LwipDataNext& p) {}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::CustomDataNext& p) {}
