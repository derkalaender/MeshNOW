#include "state.hpp"

#include <esp_event.h>
#include <freertos/FreeRTOS.h>

#include "util/mac.hpp"

namespace meshnow::state {

ESP_EVENT_DEFINE_BASE(MESHNOW_INTERNAL);

static bool root{false};

static util::MacAddr root_mac;

static esp_event_loop_handle_t event_handle{nullptr};

static State state{State::DISCONNECTED_FROM_PARENT};

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

static void fireEvent(esp_event_base_t base, int32_t id, void* data, size_t data_size) {
    ESP_ERROR_CHECK(esp_event_post_to(event_handle, base, id, data, data_size, portMAX_DELAY));
    // run event loop
    ESP_ERROR_CHECK(esp_event_loop_run(event_handle, 0));
}

void setState(State new_state) {
    state = new_state;
    fireEvent(MESHNOW_INTERNAL, static_cast<int32_t>(InternalEvent::STATE_CHANGED), &state, sizeof(state));
}

State getState() { return state; }

void setRoot(bool is_root) { root = is_root; }

bool isRoot() { return root; }

void setRootMac(util::MacAddr mac) { root_mac = mac; }

util::MacAddr getRootMac() {
    // only if we can reach the root the saved mac will be valid
    assert(state == State::REACHES_ROOT);
    return root_mac;
}

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