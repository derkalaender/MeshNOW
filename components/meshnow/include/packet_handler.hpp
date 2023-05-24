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

    virtual bool handlePacket(const ReceiveMeta& meta, const packets::Payload& p) = 0;
};

/**
 * Trait which allows derived classes to define handling methods for any packet they wish.
 * The handle methods should have the signature `bool handle(const ReceiveMeta&, const SomePayload&)`.
 * They return true if handled successfully and false if another handler should handle the payload.
 * @tparam T the derived class
 */
template <typename T>
class PacketHandlerTrait : public PacketHandler {
   public:
    bool handlePacket(const ReceiveMeta& meta, const packets::Payload& p) override {
        // handler takes payload as parameter
        auto handle = [&](auto& p) {
            // check that there exists a handle method that excepts ReceiveMeta and correct payload and returns bool
            constexpr bool has_handle_func = requires(T t) {
                { t.handle(meta, p) } -> std::same_as<bool>;
            };

            // if a such method exists call it on the derived class, otherwise return false per default
            if constexpr (has_handle_func) {
                return static_cast<T&>(*this).handle(meta, p);
            } else {
                return false;
            }
        };
        // call the handle closure using visitor with the payload
        return std::visit(handle, p);
    }
};

}  // namespace meshnow