#include "event_internal.hpp"

namespace meshnow::event {

ESP_EVENT_DEFINE_BASE(MESHNOW_INTERNAL);

static esp_event_loop_handle_t event_handle{nullptr};

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

void deinit() {
    ESP_ERROR_CHECK(esp_event_loop_delete(event_handle));
    event_handle = nullptr;
}

void fireEvent(esp_event_base_t base, int32_t id, void* data, size_t data_size) {
    ESP_ERROR_CHECK(esp_event_post_to(event_handle, base, id, data, data_size, portMAX_DELAY));
    // run event loop
    ESP_ERROR_CHECK(esp_event_loop_run(event_handle, 0));
}

esp_event_loop_handle_t getEventHandle() {
    assert(event_handle);
    return event_handle;
}

}  // namespace meshnow::event