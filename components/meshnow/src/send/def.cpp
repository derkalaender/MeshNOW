#include "def.hpp"

#include "layout.hpp"
#include "util/lock.hpp"

namespace meshnow::send {

class NeighborsSingleTry : public SendBehavior {
    void send(const SendSink& sink, const packets::Payload& payload) override {
        util::Lock lock{routing::getMtx()};

        const auto& layout = routing::getLayout();

        // send to children
        for (const auto& child : layout.children) {
            sink.accept(child.mac, payload);
        }

        // send to parent
        if (layout.parent) {
            sink.accept(layout.parent->mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> meshnow::send::SendBehavior::neighborsSingleTry() {
    return std::make_unique<NeighborsSingleTry>();
}

class Parent : public SendBehavior {
    void send(const SendSink& sink, const packets::Payload& payload) override {
        // TODO retry until success
        util::Lock lock{routing::getMtx()};

        const auto& layout = routing::getLayout();

        // send to parent
        if (layout.parent) {
            sink.accept(layout.parent->mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> SendBehavior::parent() { return std::make_unique<Parent>(); }

class Children : public SendBehavior {
    void send(const SendSink& sink, const packets::Payload& payload) override {
        // TODO retry until success
        util::Lock lock{routing::getMtx()};

        const auto& layout = routing::getLayout();

        // send to children
        for (const auto& child : layout.children) {
            sink.accept(child.mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> SendBehavior::children() { return std::make_unique<Children>(); }

class Direct : public SendBehavior {
   public:
    explicit Direct(const util::MacAddr& dest_addr) : dest_addr_{dest_addr} {}

    void send(const SendSink& sink, const packets::Payload& payload) override { sink.accept(dest_addr_, payload); }

   private:
    const util::MacAddr dest_addr_;
};

std::unique_ptr<SendBehavior> SendBehavior::direct(const util::MacAddr& dest_addr) {
    return std::make_unique<Direct>(dest_addr);

}  // namespace meshnow::send
