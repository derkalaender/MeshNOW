#include "espnow-test.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

static const char *TAG = "espnow-test";

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

// random data to send
// just in case there is some shortcut/compression by the wifi driver/espnow
static const uint8_t random[ESP_NOW_MAX_DATA_LEN];

static uint32_t packet_number = 0;

static void receive_callback(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    const test_header_t *header = (const test_header_t *)data;
    if (header->ack) {
        ESP_LOGI(TAG, "Received ACK for packet %u", header->packet_number);
        // only if the packet number matches, we set the bit (it was already increased by one)
        if (header->packet_number == packet_number - 1) {
            xEventGroupSetBits(ack_event_group, ACK_RECEIVED_BIT);
        }
    } else {
        ESP_LOGI(TAG, "Received packet %u", header->packet_number);
        // send ACK
        test_packet_t packet = {
            .header =
                {
                    .packet_number = header->packet_number,
                    .timestamp = header->timestamp,
                    .ack = true,
                },
        };
        memcpy(packet.random, random, sizeof(random));
        // we ignore errors here
        esp_now_send(mac_addr, (uint8_t *)&packet, sizeof(packet));
    }
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
    esp_fill_random(random, sizeof(random));

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
    esp_now_register_recv_cb(receive_callback);
}

void espnow_test_perform(uint8_t messages) {
    uint32_t packets_sent = 0;
    uint8_t packets_successful = 0;
    uint32_t total_time = 0;  // total roundtrip time in microseconds

    while (packets_successful < messages) {
        // send packet
        test_packet_t packet = {
            .header =
                {
                    .packet_number = packet_number++,
                    .timestamp = esp_timer_get_time(),
                    .ack = false,
                },
        };
        memcpy(packet.random, random, sizeof(random));

        esp_err_t err = esp_now_send(broadcast_mac, (uint8_t *)&packet, sizeof(packet));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send packet %u: %s", packet.header.packet_number, esp_err_to_name(err));
            continue;
        }

        packets_sent++;

        // wait for ack
        EventBits_t bits =
            xEventGroupWaitBits(ack_event_group, ACK_RECEIVED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(ACK_TIMEOUT_MS));
        if (bits & ACK_RECEIVED_BIT) {
            packets_successful++;
            total_time += esp_timer_get_time() - packet.header.timestamp;
        } else {
            ESP_LOGE(TAG, "Failed to receive ACK for packet %u", packet.header.packet_number);
        }
    }

    // summary
    uint32_t lost_packets = packets_sent - packets_successful;
    uint32_t lost_percent = lost_packets * 100 / packets_sent;
    uint32_t avg_time_ms = total_time / packets_successful / 1000 / 2;  // divide by 2 because we measure roundtrip
    uint32_t bytes_per_second = packets_successful * sizeof(test_packet_t) * 1000 / avg_time_ms;

    ESP_LOGI(TAG, "=== Summary ===");
    ESP_LOGI(TAG, "Packets sent: %u", packets_sent);
    ESP_LOGI(TAG, "Packets successful: %u", packets_successful);
    ESP_LOGI(TAG, "Packets lost: %u (%u%%)", lost_packets, lost_percent);
    ESP_LOGI(TAG, "Average time: %u ms", avg_time_ms);
    ESP_LOGI(TAG, "Bytes per second: %u", bytes_per_second);
    ESP_LOGI(TAG, "================");
}
