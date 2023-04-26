#pragma once

#include <cstdint>
#include <vector>

#include "constants.hpp"

namespace MeshNOW {
class Networking {
   public:
    /**
     * Broadcasts a raw payload to all nearby devices, no matter if connected/part of the mesh or not.
     * @param payload data to send
     *
     * @note Payloads larger than MAX_RAW_PAYLOAD_SIZE will throw an exception.
     */
    static void raw_broadcast(const std::vector<uint8_t>& payload);

    /**
     * Sends a raw payload to a specific device (ESP-NOW wrapper).
     * @param mac_addr the MAC address of the device to send to
     * @param payload data to send
     *
     * @note Payloads larger than MAX_RAW_PAYLOAD_SIZE will throw an exception.
     */
    static void raw_send(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& payload);
};
}  // namespace MeshNOW