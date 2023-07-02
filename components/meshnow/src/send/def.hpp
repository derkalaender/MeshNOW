#pragma once

#include "packets.hpp"
#include "util/mac.hpp"

namespace meshnow::send {

/**
 * A sink accepts packets to be sent and does the actual underlying sending.
 */
class SendSink {
   public:
    virtual ~SendSink() = default;

    /**
     * "Consumes" a packet for sending.
     * @param dest_addr Where to send the packet to.
     * @param packet The packet to send.
     * @return True, iff sending was successful.
     */
    virtual bool accept(const util::MacAddr& dest_addr, const packets::Packet& packet) = 0;
};

/**
 * Defines how a packet is sent, e.g., to which nodes, are addresses resolved etc.
 */
class SendBehavior {
   public:
    virtual ~SendBehavior() = default;

    virtual void send(const SendSink& sink, const packets::Packet& packet) = 0;
};

}  // namespace meshnow::send