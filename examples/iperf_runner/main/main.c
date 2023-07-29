#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <iperf.h>
#include <lwip/ip4_addr.h>
#include <lwip/lwip_napt.h>
#include <lwip/prot/ip.h>
#include <meshnow.h>
#include <mqtt_client.h>
#include <nvs_flash.h>

// The Wi-Fi credentials are configured via sdkconfig
#define ROUTER_SSID CONFIG_EXAMPLE_ROUTER_SSID
#define ROUTER_PASS CONFIG_EXAMPLE_ROUTER_PASSWORD

#define IPERF_SERVER_IP

// Waitbits
#define GOT_IP_BIT BIT0

static const char *TAG = "iperf";

// Fixed MAC address of the root node
static const uint8_t root_mac[MESHNOW_ADDRESS_LENGTH] = {0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

// MAC address of the current node
static uint8_t node_mac[MESHNOW_ADDRESS_LENGTH];

// Whether this node is the root
static bool is_root = false;

// Event group for various waiting processes
static EventGroupHandle_t my_event_group;

// Event handler for IP_EVENT
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == IP_EVENT);
    assert(event_id == IP_EVENT_STA_GOT_IP);

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    // set the bit for getting IP
    xEventGroupSetBits(my_event_group, GOT_IP_BIT);
}

static uint32_t get_ip() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey((ESP_NETIF_BASE_DEFAULT_WIFI_STA)->if_key);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    return ip_info.ip.addr;
}

static void open_port() {
    ip4_addr_t my_ip = {.addr = get_ip()};
    ip4_addr_t target_ip;
    IP4_ADDR(&target_ip, 10, 0, 0, 2);
    ESP_LOGI(TAG, "Adding port mapping " IPSTR ":%d -> " IPSTR ":%d", IP2STR(&my_ip), IPERF_DEFAULT_PORT,
             IP2STR(&target_ip), IPERF_DEFAULT_PORT);
    ip_portmap_add(IP_PROTO_TCP, my_ip.addr, IPERF_DEFAULT_PORT, target_ip.addr, IPERF_DEFAULT_PORT);
}

static void perform_iperf() {
    ESP_LOGI(TAG, "Starting iperf server");

    iperf_cfg_t cfg;
    cfg.flag = IPERF_FLAG_SERVER;
    // TCP since we want a measurement for MQTT, HTTP, etc.
    cfg.flag |= IPERF_FLAG_TCP;
    // only IPv4 is supported
    cfg.type = IPERF_IP_TYPE_IPV4;
    // set IP addresses
    //    if (!is_root) {
    //        ip4_addr_t dest;
    //        IP4_ADDR(&dest, 192, 168, 178, 43);
    //        cfg.destination_ip4 = ip4_addr_get_u32(&dest);
    //    }
    //    if (!is_root) {
    //        ip4_addr_t src;
    //        IP4_ADDR(&src, 192, 168, 137, 3);
    //        cfg.source_ip4 = ip4_addr_get_u32(&src);
    //    }
    cfg.source_ip4 = get_ip();
    // default port
    cfg.dport = cfg.sport = IPERF_DEFAULT_PORT;

    // default measure settings
    cfg.interval = 1;
    cfg.time = 10;
    cfg.len_send_buf = 0;
    cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;

    iperf_start(&cfg);
}

// Initializes NVS, Event Loop, Wi-Fi, and Netif
static void pre_init(void) {
    // nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // netif
    ESP_ERROR_CHECK(esp_netif_init());
}

void app_main(void) {
    pre_init();

    // get MAC address
    ESP_ERROR_CHECK(esp_read_mac(node_mac, ESP_MAC_WIFI_STA));

    // check if root
    is_root = memcmp(node_mac, root_mac, MESHNOW_ADDRESS_LENGTH) == 0;

    // create event group
    my_event_group = xEventGroupCreate();
    assert(my_event_group != NULL);

    // initialize meshnow
    wifi_sta_config_t sta_config = {
        .ssid = ROUTER_SSID,
        .password = ROUTER_PASS,
    };
    meshnow_config_t config = {
        .root = is_root,
        .router_config =
            {
                .should_connect = true,  // MQTT requires internet access
                .sta_config = &sta_config,
            },
    };
    ESP_ERROR_CHECK(meshnow_init(&config));

    // register event handler for IP_EVENT
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));

    // start meshnow
    ESP_ERROR_CHECK(meshnow_start());

    // wait for IP
    // when we get an IP address, the node has to have connected to a parent/router
    // in a real application, you would want to handle disconnects/lost IP as well and restart MQTT
    EventBits_t bits = xEventGroupWaitBits(my_event_group, GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    assert((bits & GOT_IP_BIT) != 0);

    if (is_root) {
        open_port();
    }

    if (!is_root) {
        perform_iperf();
    }
}
