#include "led_pattern.h"

// Warm amber: red with some green, no blue.
static const led_rgb_t k_amber = {LED_PATTERN_MAX_LEVEL, LED_PATTERN_MAX_LEVEL * 2 / 5, 0};
static const led_rgb_t k_off = {0, 0, 0};

#define BLINK_LEVEL   (LED_PATTERN_MAX_LEVEL / 4)
#define SWEEP_LEVEL   (LED_PATTERN_MAX_LEVEL * 5 / 16)
#define WAITING_LEVEL (LED_PATTERN_MAX_LEVEL / 8)
#define ERROR_LEVEL   (LED_PATTERN_MAX_LEVEL * 3 / 4)
#define SWEEP_LEDS    8
// The dot fades out over this distance (in thousandths of an LED), so about three LEDs glow at once.
#define SWEEP_WIDTH   1500

// Triangle wave 0..255..0 over one pulse period, squared so the fade looks even to the eye.
static uint8_t pulse_level(uint32_t now_ms)
{
    uint32_t phase = now_ms % LED_PATTERN_PULSE_PERIOD_MS;
    uint32_t half = LED_PATTERN_PULSE_PERIOD_MS / 2;
    uint32_t tri = phase < half ? phase * 255 / half : (LED_PATTERN_PULSE_PERIOD_MS - phase) * 255 / half;
    return (uint8_t)(tri * tri * LED_PATTERN_MAX_LEVEL / (255 * 255));
}

// A dot that moves from LED 0 to LED 7 and back once per period, with a soft edge.
static uint8_t sweep_level(int slot, uint32_t now_ms)
{
    uint32_t phase = now_ms % LED_PATTERN_SWEEP_PERIOD_MS;
    uint32_t half = LED_PATTERN_SWEEP_PERIOD_MS / 2;
    uint32_t span = (SWEEP_LEDS - 1) * 1000;
    uint32_t pos = phase < half ? phase * span / half : (LED_PATTERN_SWEEP_PERIOD_MS - phase) * span / half;
    uint32_t here = (uint32_t)slot * 1000;
    uint32_t dist = here > pos ? here - pos : pos - here;
    return dist >= SWEEP_WIDTH ? 0 : (uint8_t)(SWEEP_LEVEL * (SWEEP_WIDTH - dist) / SWEEP_WIDTH);
}

led_rgb_t led_pattern_color(led_slot_state_t state, int slot, uint32_t now_ms)
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
    case LED_SLOT_STARTING: {
        uint8_t level = slot >= 0 && slot < SWEEP_LEDS ? sweep_level(slot, now_ms) : 0;
        return (led_rgb_t){level, level, level};
    }
    case LED_SLOT_WAITING:
        return (led_rgb_t){WAITING_LEVEL, WAITING_LEVEL, WAITING_LEVEL};
    case LED_SLOT_RADIO_FAILED:
        return (led_rgb_t){ERROR_LEVEL, 0, 0};
    case LED_SLOT_EMPTY:
    case LED_SLOT_CLOSED:
    default:
        return k_off;
    }
}
