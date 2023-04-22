#include "espnow_test.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

static const char *TAG = "espnow-test";

static const char *NVS_NAMESPACE = "espnow-test";

static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct {
    uint32_t packet_number;
    uint32_t timestamp;
    bool ack;
} __attribute__((packed)) test_header_t;

typedef struct {
    test_header_t header;
    uint8_t random[ESP_NOW_MAX_DATA_LEN - sizeof(test_header_t)];
} __attribute__((packed)) test_packet_t;

// waitbits for ack
static EventGroupHandle_t ack_event_group;
static const int ACK_RECEIVED_BIT = BIT0;
static const int SEND_CB_BIT = BIT1;

// random data to send
// just in case there is some shortcut/compression by the wifi driver/espnow
static uint8_t data[ESP_NOW_MAX_DATA_LEN];

static uint32_t packet_number = 0;
static uint32_t received_time = 0;

static void receive_callback(const struct esp_now_recv_info *info, const uint8_t *data, int data_len) {
    const test_header_t *header = (const test_header_t *)data;
    if (header->ack) {
        ESP_LOGI(TAG, "Received ACK for packet %lu", header->packet_number);
        // only if the packet number matches, we set the bit (it was already increased by one)
        if (header->packet_number == packet_number - 1) {
            // update the time here because eventbits take some time
            received_time = esp_timer_get_time();
            xEventGroupSetBits(ack_event_group, ACK_RECEIVED_BIT);
        }
    } else {
        ESP_LOGI(TAG, "Received packet %lu", header->packet_number);
        // send ACK
        test_packet_t packet = {
            .header =
                {
                    .packet_number = header->packet_number,
                    .timestamp = header->timestamp,
                    .ack = true,
                },
        };
        memcpy(packet.random, data, sizeof(packet.random));
        // we ignore errors here
        esp_now_send(broadcast_mac, (uint8_t *)&packet, sizeof(packet));
    }
}

static void send_callback(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Send success");
    } else {
        ESP_LOGE(TAG, "Send failed");
    }
    xEventGroupSetBits(ack_event_group, SEND_CB_BIT);
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());                 // initialize the tcp stack
    ESP_ERROR_CHECK(esp_event_loop_create_default());  // create default event loop

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // no powersaving
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // use long range mode -> up to 1km according to espressif
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR));

    ESP_LOGI(TAG, "WiFi initialized");
}

static void nvs_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "NVS initialized");
}

void espnow_test_init(void) {
    nvs_init();
    wifi_init();

    // once wifi is initialized, we have random entropy
    esp_fill_random((void *)data, sizeof(data));

    // init event bits
    ack_event_group = xEventGroupCreate();
    if (ack_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create ack event group");
        return;
    }

    ESP_ERROR_CHECK(esp_now_init());

    // add broadcast as peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, 6);
    esp_now_add_peer(&peer_info);

    // register callbacks
    esp_now_register_recv_cb(&receive_callback);
    esp_now_register_send_cb(&send_callback);
}

void espnow_test_perform(uint8_t messages) {
    uint32_t packets_sent = 0;
    uint8_t packets_successful = 0;
    uint32_t total_time = 0;  // total roundtrip time in microseconds

    ESP_LOGI(TAG, "Starting test with %d messages", messages);

    while (packets_successful < messages) {
        ESP_LOGI(TAG, "Sending packet %lu", packet_number);

        // send packet
        test_packet_t packet = {
            .header =
                {
                    .packet_number = packet_number++,
                    .timestamp = 0,
                    .ack = false,
                },
        };
        memcpy(packet.random, data, sizeof(packet.random));

        // write timestamp after copying the data to avoid wrong timing
        packet.header.timestamp = esp_timer_get_time();

        esp_err_t err = esp_now_send(broadcast_mac, (uint8_t *)&packet, sizeof(packet));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send packet %lu: %s", packet.header.packet_number, esp_err_to_name(err));
            continue;
        }

        packets_sent++;

        // wait for ack
        EventBits_t bits = xEventGroupWaitBits(ack_event_group, ACK_RECEIVED_BIT | SEND_CB_BIT, pdTRUE, pdTRUE,
                                               pdMS_TO_TICKS(ACK_TIMEOUT_MS));
        if (!(bits & SEND_CB_BIT)) {
            ESP_LOGE(TAG, "Send callback not called");
            packets_sent--;  // mistake, we didn't actually send the packet
        } else if (bits & ACK_RECEIVED_BIT) {
            packets_successful++;
            total_time += received_time - packet.header.timestamp;
        } else {
            ESP_LOGE(TAG, "Failed to receive ACK for packet %lu", packet.header.packet_number);
        }
    }

    // summary
    total_time /= 2;  // roundtrip

    uint32_t lost_packets = packets_sent - packets_successful;
    uint32_t lost_percent = lost_packets * 100 / packets_sent;
    double avg_time_ms = (double)total_time / packets_successful / 1000;
    uint32_t bytes_per_second = (uint32_t)(packets_successful * sizeof(test_packet_t) / avg_time_ms * 1000);

    ESP_LOGI(TAG, "=== Summary ===");
    ESP_LOGI(TAG, "Total time: %luus", total_time);
    ESP_LOGI(TAG, "Packets sent: %lu", packets_sent);
    ESP_LOGI(TAG, "Packets successful: %u", packets_successful);
    ESP_LOGI(TAG, "Packets lost: %lu (%lu%%)", lost_packets, lost_percent);
    ESP_LOGI(TAG, "Average time: %.2fms", avg_time_ms);
    ESP_LOGI(TAG, "Bytes per second: %lu", bytes_per_second);
    ESP_LOGI(TAG, "================");

    ESP_LOGI(TAG, "Saving results to NVS");

    // SAVE TO NVS

    // get handle to nvs
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    // we append our reading, for this we need to get the last index
    uint32_t index = 0;
    esp_err_t ret = nvs_get_u32(handle, "index", &index);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No previous results found");
        ESP_ERROR_CHECK(nvs_set_u32(handle, "index", 0));
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_LOGI(TAG, "Writing results to index %lu", index);

    // write results
    char key[16] = {0};
    sprintf(key, "%lu", index);
    espnow_test_result_t result = {
        .sent = packets_sent,
        .successful = packets_successful,
        .time_taken = total_time,
    };
    ESP_ERROR_CHECK(nvs_set_blob(handle, key, &result, sizeof(result)));
    // increment index
    ESP_ERROR_CHECK(nvs_set_u32(handle, "index", index + 1));
    // commit
    ESP_ERROR_CHECK(nvs_commit(handle));

    // close
    nvs_close(handle);

    ESP_LOGI(TAG, "Done");
}
