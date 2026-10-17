#include "led_pattern.h"

// Warm amber: red with some green, no blue.
static const led_rgb_t k_amber = {LED_PATTERN_MAX_LEVEL, LED_PATTERN_MAX_LEVEL * 2 / 5, 0};
static const led_rgb_t k_off = {0, 0, 0};

#define BLINK_LEVEL   (LED_PATTERN_MAX_LEVEL / 4)
#define RING_LEVEL    (LED_PATTERN_MAX_LEVEL * 5 / 16)
#define WAITING_LEVEL (LED_PATTERN_MAX_LEVEL / 8)
#define ERROR_LEVEL   (LED_PATTERN_MAX_LEVEL * 3 / 4)
// Distances are in thousandths of an LED. The ring is soft over this distance on each side of its radius,
// grows to the corners (about 4950 from the centre) and a little beyond, so the corners fade out too.
#define RING_WIDTH    1000
#define RING_END      5600
// The ring fades in over the first eighth of the period and out over the rest.
#define RING_FADE_IN  (LED_PATTERN_RING_PERIOD_MS / 8)

// Triangle wave 0..255..0 over one pulse period, squared so the fade looks even to the eye.
static uint8_t pulse_level(uint32_t now_ms)
{
    uint32_t phase = now_ms % LED_PATTERN_PULSE_PERIOD_MS;
    uint32_t half = LED_PATTERN_PULSE_PERIOD_MS / 2;
    uint32_t tri = phase < half ? phase * 255 / half : (LED_PATTERN_PULSE_PERIOD_MS - phase) * 255 / half;
    return (uint8_t)(tri * tri * LED_PATTERN_MAX_LEVEL / (255 * 255));
}

static uint32_t isqrt(uint32_t n)
{
    uint32_t root = 0, bit = 1u << 30;
    while (bit > n) {
        bit >>= 2;
    }
    while (bit) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

led_rgb_t led_pattern_startup(int x, int y, uint32_t now_ms)
{
    if (x < 0 || x >= LED_PATTERN_PANEL_SIZE || y < 0 || y >= LED_PATTERN_PANEL_SIZE) {
        return k_off;
    }
    uint32_t phase = now_ms % LED_PATTERN_RING_PERIOD_MS;
    uint32_t radius = phase * RING_END / LED_PATTERN_RING_PERIOD_MS;
    uint32_t fade = phase < RING_FADE_IN ? phase * 1000 / RING_FADE_IN
                                         : (LED_PATTERN_RING_PERIOD_MS - phase) * 1000 /
                                               (LED_PATTERN_RING_PERIOD_MS - RING_FADE_IN);
    // Distance from the panel's centre, which lies between the four middle LEDs.
    int32_t dx = (2 * x - (LED_PATTERN_PANEL_SIZE - 1)) * 500;
    int32_t dy = (2 * y - (LED_PATTERN_PANEL_SIZE - 1)) * 500;
    uint32_t dist = isqrt((uint32_t)(dx * dx + dy * dy));
    uint32_t off_ring = dist > radius ? dist - radius : radius - dist;
    if (off_ring >= RING_WIDTH) {
        return k_off;
    }
    uint8_t level = (uint8_t)(RING_LEVEL * (RING_WIDTH - off_ring) * fade / (RING_WIDTH * 1000));
    return (led_rgb_t){level, level, level};
}

led_rgb_t led_pattern_color(led_slot_state_t state, uint32_t now_ms)
{
    switch (state) {
    case LED_SLOT_OPEN:
        return k_amber;
    case LED_SLOT_PAIRING:
        return (led_rgb_t){0, pulse_level(now_ms), 0};
    case LED_SLOT_UNREACHABLE:
        if (now_ms % LED_PATTERN_BLINK_PERIOD_MS < LED_PATTERN_BLINK_ON_MS) {
            return (led_rgb_t){0, 0, BLINK_LEVEL};
        }
        return k_off;
    case LED_SLOT_WAITING:
        return (led_rgb_t){WAITING_LEVEL, WAITING_LEVEL, WAITING_LEVEL};
    case LED_SLOT_RADIO_FAILED:
        return (led_rgb_t){ERROR_LEVEL, 0, 0};
    case LED_SLOT_EMPTY:
    case LED_SLOT_CLOSED:
    case LED_SLOT_STARTING:
    default:
        return k_off;
    }
}
