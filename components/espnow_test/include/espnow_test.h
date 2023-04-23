#pragma once

#include <stdint.h>

#define ACK_TIMEOUT_MS (30)

typedef struct {
    uint32_t sent;
    uint32_t successful;
    uint32_t time_taken;
} espnow_test_result_t;

/**
 * @brief Initialize network and espnow for the tests.
 */
void espnow_test_init(void);

/**
 * @brief Perform a test with the given number of messages.
 *
 * The messages will all be sent using broadcast to avoid espnow ACKs and retries and instead use our own mechanism.
 * Counts the number of total messages sent, received and lost. Also averages the time it takes to send a message
 * (roundtrip / 2).
 * Saves the results to NVS.
 *
 * @param messages The number of messages to send.
 */
void espnow_test_perform(uint8_t messages);
