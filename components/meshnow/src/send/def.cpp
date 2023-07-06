#include "def.hpp"

#include "layout.hpp"
#include "util/lock.hpp"

namespace meshnow::send {

class NeighborsSingleTry : public SendBehavior {
    void send(SendSink& sink, const packets::Payload& payload) override {
        auto& layout = layout::Layout::get();

        // send to children
        for (const auto& child : layout.getChildren()) {
            sink.accept(child.mac, payload);
        }

        // send to parent
        if (auto& parent = layout.getParent()) {
            sink.accept(parent->mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> meshnow::send::SendBehavior::neighborsSingleTry() {
    return std::make_unique<NeighborsSingleTry>();
}

class Parent : public SendBehavior {
    void send(SendSink& sink, const packets::Payload& payload) override {
        // TODO retry until success

        auto& layout = layout::Layout::get();

        // send to parent
        if (auto& parent = layout.getParent()) {
            sink.accept(parent->mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> SendBehavior::parent() { return std::make_unique<Parent>(); }

class Children : public SendBehavior {
    void send(SendSink& sink, const packets::Payload& payload) override {
        // TODO retry until success

        auto& layout = layout::Layout::get();

        // send to children
        for (const auto& child : layout.getChildren()) {
            sink.accept(child.mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> SendBehavior::children() { return std::make_unique<Children>(); }

class DirectSingleTry : public SendBehavior {
   public:
    explicit DirectSingleTry(const util::MacAddr& dest_addr) : dest_addr_(dest_addr) {}

    void send(SendSink& sink, const packets::Payload& payload) override {
        // don't do anything if we are the target
        if (dest_addr_ == state::getThisMac()) return;

        sink.accept(dest_addr_, payload);
    }

   private:
    const util::MacAddr dest_addr_;
};

std::unique_ptr<SendBehavior> SendBehavior::directSingleTry(const util::MacAddr& dest_addr) {
    return std::make_unique<DirectSingleTry>(dest_addr);
}

class Resolve : public SendBehavior {
   public:
    explicit Resolve(const util::MacAddr& target, const util::MacAddr& last_hop) : target_(target), except_(last_hop) {}

    /**
     * 1. If target is broadcast, send to everyone except last_hop to avoid loops
     * 2. If target is root, send to parent
     * 3. If target is parent, send to parent
     * 4. If target is an (indirect) child, send downstream to correct child
     * 5. Otherwise, target is not in layout so we send upstream to parent
     */
    void send(SendSink& sink, const packets::Payload& payload) override {
        // don't do anything if we are the target
        if (target_ == state::getThisMac()) return;

        // TODO retry until success
        auto& layout = layout::Layout::get();

        // broadcast
        if (target_.isBroadcast()) {
            // send to everyone except last_hop

            // send to children
            for (const auto& child : layout.getChildren()) {
                if (child.mac != except_) {
                    sink.accept(child.mac, payload);
                }
            }

            // send to parent
            if (auto& parent = layout.getParent(); parent && parent->mac != except_) {
                sink.accept(parent->mac, payload);
            }
        } else if (target_.isRoot()) {
            if (state::isRoot()) return;
            // send upstream until root
            if (auto& parent = layout.getParent()) {
                sink.accept(parent->mac, payload);
            }
        } else if (auto& parent = layout.getParent(); parent && parent->mac == target_) {
            // send upstream to parent
            sink.accept(parent->mac, payload);
        } else {
            // find child that either is the target or has a child that is the target
            auto children = layout.getChildren();
            auto child = std::find_if(children.begin(), children.end(), [&](const layout::Child& child) {
                return child.mac == target_ || inRoutingTable(child, target_);
            });

            if (child != children.end()) {
                // send downstream to child
                sink.accept(child->mac, payload);
            } else {
                // send upstream to parent
                if (parent) {
                    sink.accept(parent->mac, payload);
                }
            }
        }
    }

   private:
    static inline bool inRoutingTable(const layout::Child& child, const util::MacAddr& target) {
        return std::any_of(child.routing_table.begin(), child.routing_table.end(),
                           [&](const layout::Node node) { return node.mac == target; });
    }

    const util::MacAddr target_;
    const util::MacAddr except_;
};

std::unique_ptr<SendBehavior> SendBehavior::resolve(const util::MacAddr& target, const util::MacAddr& last_hop) {
    return std::make_unique<Resolve>(target, last_hop);
}

}  // namespace meshnow::send
