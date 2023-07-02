#pragma once

#include <esp_err.h>
#include <esp_event.h>

#include "state.hpp"

namespace meshnow::event {

ESP_EVENT_DECLARE_BASE(MESHNOW_INTERNAL);

enum InternalEvent {
    STATE_CHANGED,
};

struct StateChangedData {
    state::State old_state;
    state::State new_state;
};

/**
 * Initializes internal event loop.
 */
esp_err_t init();

/**
 * Deinitializes everything.
 */
void deinit();

void fireEvent(esp_event_base_t base, int32_t id, void* data, size_t data_size);

esp_event_loop_handle_t getEventHandle();

}  // namespace meshnow::event