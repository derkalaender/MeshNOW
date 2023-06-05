#include "esp_now_multi.h"

#include <assert.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_now.h>
#include <stdint.h>

static const char* TAG = "esp_now_multi";

typedef struct register_list {
    esp_now_multi_reg_t reg;
    struct register_list* next;
} register_list_t;

// linked list of all registered handlers
static register_list_t* register_list = NULL;

static void recv_cb(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    register_list_t* list = register_list;
    while (list) {
        list->reg.recv_cb(esp_now_info, data, data_len, list->reg.arg);
        list = list->next;
    }
}

static void send_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {
    register_list_t* list = register_list;
    while (list) {
        list->reg.send_cb(mac_addr, status, list->reg.arg);
        list = list->next;
    }
}

esp_err_t esp_now_multi_init() {
    // register callbacks
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(recv_cb), TAG, "Failed to register ESP-NOW receive callback");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(send_cb), TAG, "Failed to register ESP-NOW send callback");

    return ESP_OK;
}

esp_err_t esp_now_multi_deinit() {
    // free all list entries
    register_list_t* current = register_list;
    while (current != NULL) {
        register_list_t* next = current->next;
        free(current);
        current = next;
    }
    register_list = NULL;

    // unregister callbacks
    ESP_RETURN_ON_ERROR(esp_now_unregister_send_cb(), TAG, "Failed to unregister ESP-NOW send callback");
    ESP_RETURN_ON_ERROR(esp_now_unregister_recv_cb(), TAG, "Failed to unregister ESP-NOW receive callback");

    return ESP_OK;
}

esp_err_t esp_now_multi_register(esp_now_multi_reg_t reg, esp_now_multi_handle_t* handle) {
    // create new list entry
    register_list_t* new_reg = malloc(sizeof(register_list_t));
    if (!new_reg) return ESP_ERR_NO_MEM;

    new_reg->reg = reg;
    new_reg->next = NULL;

    // add list entry at the end of the list
    if (register_list == NULL) {
        register_list = new_reg;
    } else {
        register_list_t* current = register_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_reg;
    }

    *handle = new_reg;

    return ESP_OK;
}

esp_err_t esp_now_multi_unregister(esp_now_multi_handle_t handle) {
    register_list_t* current = register_list;
    register_list_t* previous = NULL;

    // find list entry
    while (current != NULL) {
        if (current == handle) {
            // remove list entry
            if (previous == NULL) {
                register_list = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return ESP_OK;
        }
        previous = current;
        current = current->next;
    }

    return ESP_ERR_NOT_FOUND;
}
