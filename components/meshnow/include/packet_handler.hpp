#pragma once

#include <variant>

#include "packets.hpp"
#include "receive_meta.hpp"

namespace meshnow {

/**
 * Abstract base class for PacketHandlers to allow them to be used in vectors.
 */
class PacketHandler {
   public:
    virtual ~PacketHandler() = default;

    virtual void handlePacket(const ReceiveMeta& meta, const packets::Payload& p) = 0;
};

/**
 * Trait which allows derived classes to define handling methods for any packet they wish.
 * The handle methods should have the signature `void handle(const ReceiveMeta&, const SomePayload&)`.
 * @tparam T the derived class
 */
template <typename T>
class PacketHandlerTrait : public PacketHandler {
   public:
    void handlePacket(const ReceiveMeta& meta, const packets::Payload& p) override {
        // handler takes payload as parameter
        auto handle = [&](auto& p) {
            // check that there exists a handle method that excepts ReceiveMeta and correct payload and returns bool
            constexpr bool has_handle_func = requires(T t) {
                { t.handle(meta, p) } -> std::same_as<void>;
            };

            // if a such method exists call it on the derived class, otherwise return false per default
            if constexpr (has_handle_func) {
                static_cast<T&>(*this).handle(meta, p);
            }
        };
        // call the handle closure using visitor with the payload
        return std::visit(handle, p);
    }
};

}  // namespace meshnow