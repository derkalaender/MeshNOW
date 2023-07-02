#pragma once

#include <esp_event.h>

#include "util/mac.hpp"

namespace meshnow::state {

ESP_EVENT_DECLARE_BASE(MESHNOW_INTERNAL);

enum MeshNOWInternalEvent {
    STATE_CHANGED,
};

enum class State : std::uint8_t { DISCONNECTED_FROM_PARENT, CONNECTED_TO_PARENT, REACHES_ROOT };

/**
 * Initializes internal event loop for state updates.
 */
esp_err_t init();

/**
 * Deinitializes everything.
 */
void deinit();

/**
 * Sets the current state.
 */
void setState(State state);

/**
 * Returns the current state.
 */
State getState();

esp_event_loop_handle_t getEventHandle();

/**
 * Set if this device is the root node of the mesh.
 */
void setRoot(bool is_root);

/**
 * Returns if this device is the root node of the mesh.
 */
bool isRoot();

void setRootMac(util::MacAddr mac);

util::MacAddr getRootMac();

/**
 * Returns the MAC address of this device.
 */
util::MacAddr getThisMac();

}  // namespace meshnow::state
