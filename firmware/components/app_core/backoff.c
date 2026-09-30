#include "backoff.h"

void backoff_init(backoff_t *b, uint32_t base_ms, uint32_t max_ms)
{
    b->base_ms = base_ms;
    b->max_ms = max_ms;
    b->attempt = 0;
}

void backoff_reset(backoff_t *b)
{
    b->attempt = 0;
}

uint32_t backoff_next(backoff_t *b, uint32_t random)
{
    uint64_t delay = b->attempt < 32 ? (uint64_t)b->base_ms << b->attempt : UINT64_MAX;
    if (delay > b->max_ms) {
        delay = b->max_ms;
    } else {
        b->attempt++; // stop counting once capped, so the shift can't overflow
    }
    // Uniform in [0.8 * delay, 1.2 * delay].
    uint64_t span = delay * 2 / 5;
    return (uint32_t)(delay - span / 2 + (span ? random % (span + 1) : 0));
}
