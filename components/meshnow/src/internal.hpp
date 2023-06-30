#pragma once

#include "util/mac.hpp"

namespace meshnow::internal {

/**
 * Set if this device is the root node of the mesh.
 */
void setRoot(bool is_root);

/**
 * Returns if this device is the root node of the mesh.
 */
bool isRoot();

/**
 * Returns the MAC address of this device.
 */
util::MacAddr getThisMac();

}  // namespace meshnow::internal
