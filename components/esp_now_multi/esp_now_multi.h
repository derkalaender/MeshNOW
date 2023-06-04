#pragma once

#include <esp_err.h>
#include <esp_now.h>

typedef void (*esp_now_multi_recv_cb_t)(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len,
                                        void *arg);

typedef void (*esp_now_multi_send_cb_t)(const uint8_t *mac_addr, esp_now_send_status_t status, void *arg);

typedef struct {
    esp_now_multi_recv_cb_t recv_cb;
    esp_now_multi_send_cb_t send_cb;
    void *arg;
} esp_now_multi_reg_t;

typedef const void *esp_now_multi_handle_t;

/**
 * Registers itself to the ESP-NOW system.
 */
esp_err_t esp_now_multi_init();

/**
 * Unregisters itself from the ESP-NOW system. Removes all handles.
 */
esp_err_t esp_now_multi_deinit();

esp_err_t esp_now_multi_register(esp_now_multi_reg_t reg, esp_now_multi_handle_t *handle);

esp_err_t esp_now_multi_unregister(esp_now_multi_handle_t handle);

esp_err_t esp_now_multi_send(esp_now_multi_handle_t handle, const uint8_t *peer_addr, const uint8_t *data, size_t len);