#include "state.hpp"

#include <esp_event.h>
#include <freertos/FreeRTOS.h>

#include "util/mac.hpp"

namespace meshnow::state {

static bool root{false};

esp_event_loop_handle_t event_handle{nullptr};

esp_err_t init() {
    {
        esp_event_loop_args_t args{
            .queue_size = 1,
            .task_name = nullptr,
        };
        if (esp_err_t ret = esp_event_loop_create(&args, &event_handle); ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

void deinit() { ESP_ERROR_CHECK(esp_event_loop_delete(event_handle)); }

static void fireEvent(esp_event_base_t base, int32_t id, void* data) {
    ESP_ERROR_CHECK(esp_event_post_to(event_handle, base, id, data, 0, portMAX_DELAY));
    // run event loop
    ESP_ERROR_CHECK(esp_event_loop_run(event_handle, 0));
}

void setRoot(bool is_root) { root = is_root; }

bool isRoot() { return root; }

util::MacAddr getThisMac() {
    // read once in the first call and cache the result
    static util::MacAddr mac{[]() {
        util::MacAddr mac;
        esp_read_mac(mac.addr.data(), ESP_MAC_WIFI_STA);
        return mac;
    }()};
    return mac;
}

}  // namespace meshnow::state