#include "def.hpp"

#include "layout.hpp"
#include "util/lock.hpp"

namespace meshnow::send {

class AllNeighbors : public SendBehavior {
    void send(const SendSink& sink, const packets::Payload& payload) override {
        util::Lock lock{routing::getMtx()};

        auto layout = routing::getLayout();

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

std::unique_ptr<SendBehavior> meshnow::send::SendBehavior::allNeighbors() { return std::make_unique<AllNeighbors>(); }

}  // namespace meshnow::send
