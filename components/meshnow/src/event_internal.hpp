#pragma once

#include <esp_err.h>
#include <esp_event.h>

#include "state.hpp"

namespace meshnow::event {

ESP_EVENT_DECLARE_BASE(MESHNOW_INTERNAL);

enum InternalEvent {
    STATE_CHANGED,
    PARENT_FOUND,
    GOT_CONNECTION_REPLY,
};

struct StateChangedData {
    const state::State old_state;
    const state::State new_state;
};

struct ParentFoundData {
    const int rssi;
    const util::MacAddr* mac;
};

struct GotConnectionReplyData {
    const bool accepted;
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