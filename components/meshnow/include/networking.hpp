#pragma once

#include <memory>

#include "main_worker.hpp"
#include "send_worker.hpp"
#include "state.hpp"

namespace meshnow {

/**
 * Handles networking.
 * <br>
 * Terminology:<br>
 * - Root: The root node is the only one potentially connected to a router and to that extend the internet. Coordinates
 * the whole network. Should be connected to a power source.<br>
 * - Node: If not otherwise specified, a "node" refers to any node that is not the root. Can be a leaf node or a branch
 * node.<br>
 * - Parent: Each node (except for the root) has exactly one parent node.<br>
 * - Child: Each node can have multiple children.<br>
 */
class Networking {
   public:
    explicit Networking(const std::shared_ptr<NodeState>& state);

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    /**
     * Starts the networking stack.
     */
    void start();

    /**
     * Stops the networking stack.
     */
    void stop();

   private:
    std::shared_ptr<routing::Layout> layout_{std::make_shared<routing::Layout>()};

   public:
    std::shared_ptr<SendWorker> send_worker_;
    std::shared_ptr<MainWorker> main_worker_;
};

}  // namespace meshnow
