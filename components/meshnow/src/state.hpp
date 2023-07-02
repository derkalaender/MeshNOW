#pragma once

#include <esp_event.h>

#include "util/mac.hpp"

namespace meshnow::state {

/**
 * Initializes internal event loop for state updates.
 */
esp_err_t init();

/**
 * Deinitializes everything.
 */
void deinit();

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
