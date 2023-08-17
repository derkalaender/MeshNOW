#pragma once

#include <esp_err.h>
#include <esp_wifi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum size (in bytes) of a custom message.
 */
#define MESHNOW_MAX_CUSTOM_MESSAGE_SIZE 230

/**
 * Length of a MAC address.
 */
#define MESHNOW_ADDRESS_LENGTH 6

/**
 * Event base for MeshNOW events.
 */
ESP_EVENT_DECLARE_BASE(MESHNOW_EVENT);

/**
 * MeshNOW event types.
 */
typedef enum {
    /**
     * A child has connected to this node.
     */
    MESHNOW_EVENT_CHILD_CONNECTED,

    /**
     * A child has disconnected from this node.
     */
    MESHNOW_EVENT_CHILD_DISCONNECTED,

    /**
     * This node has connected to a parent.
     */
    MESHNOW_EVENT_PARENT_CONNECTED,

    /**
     * This node has disconnected from a parent.
     */
    MESHNOW_EVENT_PARENT_DISCONNECTED,
} meshnow_event_t;

/**
 * Child connected information.
 */
typedef struct {
    /**
     * MAC address of the connected child.
     */
    uint8_t child_mac[MESHNOW_ADDRESS_LENGTH];
} meshnow_event_child_connected_t;

/**
 * Child disconnected information.
 */
typedef struct {
    /**
     * MAC address of the disconnected child.
     */
    uint8_t child_mac[MESHNOW_ADDRESS_LENGTH];
} meshnow_event_child_disconnected_t;

/**
 * Parent connected information.
 */
typedef struct {
    /**
     * MAC address of the parent to which this node connected.
     */
    uint8_t parent_mac[MESHNOW_ADDRESS_LENGTH];
} meshnow_event_parent_connected_t;

/**
 * Parent disconnected information.
 */
typedef struct {
    /**
     * MAC address of the parent from which this node disconnected.
     */
    uint8_t parent_mac[MESHNOW_ADDRESS_LENGTH];
} meshnow_event_parent_disconnected_t;

/**
 * Configuration options for the root when connecting to a router.
 */
typedef struct {
    /**
     * If true, the root node will try to connect to a router.
     */
    bool should_connect;

    /**
     * ESP Wi-Fi station configuration.
     */
    wifi_sta_config_t* sta_config;
} meshnow_router_config_t;

/**
 * MeshNOW configuration.
 */
typedef struct {
    /**
     * If true, this device is the root node of the mesh.
     */
    bool root;

    /**
     * Router configuration for when `root` is true.
     */
    meshnow_router_config_t router_config;
} meshnow_config_t;

/**
 * Callback for custom data packets.
 *
 * @param[in] src the address of the node that sent the packet
 * @param[in] buffer pointer to the data
 * @param[in] len length of the data
 *
 * @note The buffer is only valid for the duration of the callback. If you need to store the data, make a copy.
 *
 * @attention Do not perform any long-running or blocking operations in this callback, as it would halt the mesh.
 */
typedef void (*meshnow_data_cb_t)(uint8_t* src, uint8_t* buffer, size_t len);

/**
 * Handle for a registered data callback.
 */
typedef void* meshnow_data_cb_handle_t;

/**
 * Broadcast address.
 */
const uint8_t MESHNOW_BROADCAST_ADDRESS[MESHNOW_ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/**
 * Root address.
 */
const uint8_t MESHNOW_ROOT_ADDRESS[MESHNOW_ADDRESS_LENGTH] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * Initialize MeshNOW.
 *
 * @param[in] config MeshNOW configuration.
 *
 * @attention This API shall be called after Wi-Fi, NVS, and ESP-Netif have been initialized.
 *
 * @attention Be sure to not perform any calls to Wi-Fi before de-initializing again, unless you know what
 * you are doing.
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_ARG: Invalid config
 * - ESP_ERR_INVALID_STATE: MeshNOW is already initialized, or Wi-Fi, NVS, or ESP-Netif is not initialized
 * - ESP_ERR_NO_MEM: Out of memory
 * - Others: Fail
 */
esp_err_t meshnow_init(meshnow_config_t* config);

/**
 * De-initialize MeshNOW.
 *
 * @attention This API shall be called after stopping the mesh.
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_STATE: MeshNOW is not initialized or not stopped
 * - Others: Fail
 */
esp_err_t meshnow_deinit();

/**
 * Starts the mesh.
 *
 * - Root: if configured, will connect to the router. Note: other nodes may not connect while the root is connecting to
 * the router due to channel mismatch.
 * - Others: will connect to the next best parent node within range that is already connected to the mesh.
 *
 * @attention This API shall be called after initializing the mesh.
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_STATE: MeshNOW is not initialized or already started
 * - ESP_ERR_NO_MEM: Out of memory
 * - Others: Fail
 */
esp_err_t meshnow_start();

/**
 * Stops the mesh.
 *
 * - Root: will disconnect from the router.
 * - Others: will disconnect from its parent.
 *
 * Each node also disconnects all children.
 *
 * @attention This API shall be called after starting the mesh.
 *
 * @note This API is blocking to ensure all tasks can clean up properly. It may take a few seconds to return. The
 * calling task is put to sleep during this time as to not trigger the watchdog.
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_STATE: MeshNOW is not initialized or not started
 * - Others: Fail
 */
esp_err_t meshnow_stop();

/**
 * Send a custom data packet to any node in the mesh.
 *
 * @param[in] dest the address of the final destination of the packet
 *              - if MESHNOW_BROADCAST_ADDRESS, the message will be broadcasted to all nodes in the mesh
 *              - if MESHNOW_ROOT_ADDRESS, the message will be sent to the root node
 * @param[in] buffer pointer to the data to send
 * @param[in] len length of the data to send; cannot exceed MESHNOW_MAX_CUSTOM_MESSAGE_SIZE
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_ARG: Invalid argument
 * - ESP_ERR_INVALID_STATE: MeshNOW is not initialized or not started, or the node is disconnected from the mesh
 * - ESP_ERR_NO_MEM: Out of memory
 * - Others: Fail
 */
esp_err_t meshnow_send(uint8_t* dest, uint8_t* buffer, size_t len);

/**
 * Register a callback for custom data packets.
 *
 * @param[in] cb the callback to register
 * @param[out] handle handle for the callback
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_ARG: Invalid argument
 * - ESP_ERR_INVALID_STATE: MeshNOW is not initialized
 * - ESP_ERR_NO_MEM: Out of memory
 */
esp_err_t meshnow_register_data_cb(meshnow_data_cb_t cb, meshnow_data_cb_handle_t* handle);

/**
 * Unregister a callback for custom data packets.
 *
 * @param[in] handle handle for the callback
 *
 * @return
 * - ESP_OK: Success
 * - ESP_ERR_INVALID_ARG: Invalid argument
 * - ESP_ERR_INVALID_STATE: MeshNOW is not initialized
 */
esp_err_t meshnow_unregister_data_cb(meshnow_data_cb_handle_t handle);

#ifdef __cplusplus
};
#endif
