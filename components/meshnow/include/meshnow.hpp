#pragma once

#include <esp_now.h>

#include <memory>

#include "networking.hpp"
#include "state.hpp"

namespace meshnow {

struct Config {
    /**
     * Whether this node is the root node.
     *
     * @note There can only ever be exactly one root node in the mesh. Behavior is undefined if there are multiple.
     * @note The root node can establish a connection to a router if configured.
     */
    bool root{false};

    wifi_sta_config_t sta_config;
};

/**
 * Struct that holds callbacks that the user needs to register with ESP-NOW.
 */
struct Callbacks {
    /**
     * Required argument that needs to be passed unmodified to the callbacks
     */
    void *arg;

    /**
     * ESP-NOW receive callback with additional arg parameter that needs to be equal to the arg member of this struct
     */
    void (*recv_cb)(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len, void *arg);

    /**
     * ESP-NOW send callback with additional arg parameter that needs to be equal to the arg member of this struct
     */
    void (*send_cb)(const uint8_t *mac_addr, esp_now_send_status_t status, void *arg);
};

// TODO maybe singleton? Special copy constructor handling and stuff? -> YES, move semantics to guarantee only ever 1
// instance
/**
 * Main entrypoint for meshnow.
 */
class Mesh {
   public:
    /**
     * Initializes the Mesh.
     *
     * This includes setting up WiFi, Network Interfaces (netif), and ESP-NOW.
     *
     * @attention Replaces your current WiFi, netif and ESP-NOW configuration.
     * @attention Don't make any calls to WiFi, netif or ESP-NOW before deinitializing again, unless you know what
     * you're doing.
     */
    explicit Mesh(Config config);

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    /**
     * Deinitializes the Mesh.
     *
     * This includes resetting WiFi, Network Interfaces (netif), and ESP-NOW.
     *
     * @note Make sure the mesh is stopped before calling this function.
     *
     * @attention Doesn't restore your last WiFi, netif or ESP-NOW configurations. You need to handle that yourself.
     */
    ~Mesh();

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

    /**
     * Returns the callbacks that need to be registered with ESP-NOW.
     *
     * @note You need(!) to register these yourself with ESP-NOW. If you don't, the mesh won't work!
     *
     * @note This design is required because the user may use ESP-NOW for other purposes as well.
     *
     * @return Struct that holds the callbacks as well as additional required arguments.
     */
    Callbacks getCallbacks() const;

   private:
    const Config config_;
    std::shared_ptr<NodeState> state_;
    Networking networking_;
};

}  // namespace meshnow