#pragma once

#include <cstdint>
#include <memory>

#include "networking.hpp"

namespace MeshNOW {
struct Config {
    /**
     * Whether this node is the root node.
     *
     * @note There can only ever be exactly one root node in the mesh. Behavior is undefined if there are multiple.
     * @note The root node can establish a connection to a router if configured.
     */
    bool root{false};
};

enum class State {
    STOPPED,
    STARTED,
    CONNECTED,
};

// TODO maybe singleton? Special copy constructor handling and stuff? -> YES, move semantics to guarantee only ever 1
// instance
/**
 * Main entrypoint for MeshNOW.
 */
class App {
   public:
    /**
     * Initializes the App library.
     *
     * This includes setting up WiFi, Network Interfaces (netif), and ESP-NOW.
     *
     * @attention Replaces your current WiFi, netif and ESP-NOW configuration.
     * @attention Don't make any calls to WiFi, netif or ESP-NOW before deinitializing again, unless you know what
     * you're doing.
     */
    explicit App(Config config);

    /**
     * Deinitializes the App library.
     *
     * This includes resetting WiFi, Network Interfaces (netif), and ESP-NOW.
     *
     * @note Make sure the mesh is stopped before calling this function.
     *
     * @attention Doesn't restore your last WiFi, netif or ESP-NOW configurations. You need to handle that yourself.
     */
    ~App();

    /**
     * Starts the mesh.
     *
     * The node will try to connect to the mesh (unless it's the root node, in which case its automatically
     * "connected").
     */
    void start();

    /**
     * Stops the mesh.
     *
     * The node will disconnect from the mesh.
     *
     * @note Make sure to have actually started the mesh before calling this function.
     */
    void stop();

   private:
    const Config config;
    State state{State::STOPPED};
    Networking networking;
};

}  // namespace MeshNOW