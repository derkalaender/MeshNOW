#include "def.hpp"

#include "layout.hpp"
#include "util/lock.hpp"

namespace meshnow::send {

class NeighborsSingleTry : public SendBehavior {
    void send(const SendSink& sink, const packets::Payload& payload) override {
        util::Lock lock{layout::getMtx()};

        const auto& layout = layout::getLayout();

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
        util::Lock lock{layout::getMtx()};

        const auto& layout = layout::getLayout();

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
        util::Lock lock{layout::getMtx()};

        const auto& layout = layout::getLayout();

        // send to children
        for (const auto& child : layout.children) {
            sink.accept(child.mac, payload);
        }
    }
};

std::unique_ptr<SendBehavior> SendBehavior::children() { return std::make_unique<Children>(); }

class Direct : public SendBehavior {
   public:
    explicit Direct(const util::MacAddr& dest_addr) : dest_addr_(dest_addr) {}

    void send(const SendSink& sink, const packets::Payload& payload) override {
        // don't do anything if we are the target
        if (dest_addr_ == state::getThisMac()) return;

        sink.accept(dest_addr_, payload);
    }

   private:
    const util::MacAddr dest_addr_;
};

std::unique_ptr<SendBehavior> SendBehavior::direct(const util::MacAddr& dest_addr) {
    return std::make_unique<Direct>(dest_addr);
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
    void send(const SendSink& sink, const packets::Payload& payload) override {
        // don't do anything if we are the target
        if (target_ == state::getThisMac()) return;

        // TODO retry until success
        util::Lock lock{layout::getMtx()};

        const auto& layout = layout::getLayout();

        // broadcast
        if (target_.isBroadcast()) {
            // send to everyone except last_hop

            // send to children
            for (const auto& child : layout.children) {
                if (child.mac != except_) {
                    sink.accept(child.mac, payload);
                }
            }

            // send to parent
            if (layout.parent && layout.parent->mac != except_) {
                sink.accept(layout.parent->mac, payload);
            }
        } else if (target_.isRoot()) {
            if (state::isRoot()) return;
            // send upstream until root
            if (layout.parent.has_value()) {
                sink.accept(layout.parent->mac, payload);
            }
        } else if (layout.parent && layout.parent->mac == target_) {
            // send upstream to parent
            sink.accept(layout.parent->mac, payload);
        } else {
            // find child that either is the target or has a child that is the target
            auto child = std::find_if(layout.children.begin(), layout.children.end(),
                                      [&](const layout::DirectChild& child) { return contains(child, target_); });

            if (child != layout.children.end()) {
                // send downstream to child
                sink.accept(child->mac, payload);
            } else {
                // send upstream to parent
                if (layout.parent.has_value()) {
                    sink.accept(layout.parent->mac, payload);
                }
            }
        }
    }

   private:
    static bool contains(const auto& child, const util::MacAddr& target) {
        if (child.mac == target) return true;

        return std::any_of(
            child.children.begin(), child.children.end(),
            [&](const layout::IndirectChild& indirect_child) { return contains(indirect_child, target); });
    }

    const util::MacAddr target_;
    const util::MacAddr except_;
};

std::unique_ptr<SendBehavior> SendBehavior::resolve(const util::MacAddr& target, const util::MacAddr& last_hop) {
    return std::make_unique<Resolve>(target, last_hop);
}

}  // namespace meshnow::send
