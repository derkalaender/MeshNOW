#pragma once

#include <cstdint>

#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::fragments {

/**
 * Initializes fragment handling.
 */
void init();

/**
 * Deinitializes fragment handling.
 */
void deinit();

/**
 * Add a fragment for processing.
 *
 * @param src_mac MAC of the node the fragment originated from
 * @param fragment_id Random ID to identify which fragments belong together
 * @param fragment_number Number of this fragment in the sequence of fragments [0, 7]
 * @param total_size Total size of the data over all fragments in bytes [0, 1500]
 * @param data Data of this fragment
 */
void addFragment(const util::MacAddr& src_mac, uint16_t fragment_id, uint16_t fragment_number, uint16_t total_size,
                 util::Buffer data);

void popReassembledData();

}  // namespace meshnow::fragments