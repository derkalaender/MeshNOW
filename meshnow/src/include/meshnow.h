#pragma once

#include <esp_err.h>
#include <esp_wifi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Event base for MeshNOW events.
 */
ESP_EVENT_DECLARE_BASE(MESHNOW_EVENT);

/**
 * MeshNOW event types.
 */
typedef enum {
    // maybe add start & stop back in?
    // MESH_EVENT_STARTED,
    // MESH_EVENT_STOPPED,
    MESHNOW_EVENT_CHILD_CONNECTED,
    MESHNOW_EVENT_CHILD_DISCONNECTED,
    MESHNOW_EVENT_PARENT_CONNECTED,
    MESHNOW_EVENT_PARENT_DISCONNECTED,
    // TODO routing table
} meshnow_event_t;

// MeshNOW event data structs

typedef struct {
    uint8_t child_mac[6];
} meshnow_event_child_connected_t;

typedef struct {
    uint8_t child_mac[6];
} meshnow_event_child_disconnected_t;

typedef struct {
    uint8_t parent_mac[6];
} meshnow_event_parent_connected_t;

typedef struct {
    uint8_t parent_mac[6];
} meshnow_event_parent_disconnected_t;

/**
 * MeshNOW configuration struct.
 */
typedef struct {
    /**
     * If true, this device is the root node of the mesh.
     */
    bool root;

    /**
     * Configuration options for the root when connecting to a router.
     */
    struct {
        /**
         * If true, the root node will try to should_connect to a router.
         */
        bool should_connect;

        /**
         * ESP Wi-Fi station configuration.
         */
        wifi_sta_config_t* sta_config;
    } router_config;
} meshnow_config_t;

/**
 * Initializes MeshNOW.
 *
 * @param config MeshNOW configuration.
 *
 * @note Expects Wi-Fi and ESP Netif to be initialized already.
 *
 * @attention Replaces your current WiFi, netif and ESP-NOW configuration.
 * @attention Don't make any calls to WiFi, netif or ESP-NOW before deinitializing again, unless you know what
 * you're doing.
 */
esp_err_t meshnow_init(meshnow_config_t* config);

/**
 * Deinitializes MeshNOW.
 *
 * @note Make sure the mesh is stopped before calling this function.
 */
esp_err_t meshnow_deinit();

/**
 * Starts the mesh.
 *
 * The node will try to should_connect to the mesh (unless it's the root node, in which case its automatically
 * "connected").
 */
esp_err_t meshnow_start();

/**
 * Stops the mesh.
 *
 * The node will disconnect from the mesh.
 *
 * @note Make sure to have actually started the mesh before calling this function.
 */
esp_err_t meshnow_stop();

// TODO custom data

#ifdef __cplusplus
};
#endif