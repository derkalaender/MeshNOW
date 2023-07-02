#pragma once

#include <memory>

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
     * "Consumes" a payload for sending.
     * @param dest_addr Where to send the packet to.
     * @param payload The payload to send.
     * @return True, iff sending was successful.
     */
    virtual bool accept(const util::MacAddr& dest_addr, const packets::Payload& payload) const = 0;
};

/**
 * Defines how a packet is sent, e.g., to which nodes, are addresses resolved etc.
 */
class SendBehavior {
   public:
    static std::unique_ptr<SendBehavior> allNeighbors();

    virtual ~SendBehavior() = default;

    /**
     * Sends the given payload by calling the sink.
     * @param sink The sink to send the payload to.
     * @param payload The payload to send.
     */
    virtual void send(const SendSink& sink, const packets::Payload& payload) = 0;
};

}  // namespace meshnow::send