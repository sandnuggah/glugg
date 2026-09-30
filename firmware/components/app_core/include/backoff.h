#pragma once

// Exponential backoff with +/-20% jitter and no retry limit (esp-matter's own resubscribe gives up after 2).

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t base_ms;
    uint32_t max_ms;
    uint8_t attempt;
} backoff_t;

void backoff_init(backoff_t *b, uint32_t base_ms, uint32_t max_ms);
void backoff_reset(backoff_t *b);

// Delay before the next attempt after a failure: base * 2^attempt, capped at max, with jitter from `random`
// (any uniformly distributed 32-bit value, e.g. esp_random()).
uint32_t backoff_next(backoff_t *b, uint32_t random);

#ifdef __cplusplus
}
#endif
