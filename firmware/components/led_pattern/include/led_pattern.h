#pragma once

// Maps a slot's state and the current time to an LED colour, and draws the startup animation on the 8x8 panel.
// Pure C with no ESP-IDF or Matter dependencies, so it builds and tests on the host.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_SLOT_EMPTY,        // no sensor paired: off
    LED_SLOT_CLOSED,       // window closed: off
    LED_SLOT_OPEN,         // window open: warm amber, steady
    LED_SLOT_PAIRING,      // pairing into this slot: slow green pulse
    LED_SLOT_UNREACHABLE,  // sensor not heard from: dim blue blink
    LED_SLOT_STARTING,     // network not up yet: the whole panel shows led_pattern_startup() instead
    LED_SLOT_WAITING,      // paired, but no report since boot or pairing: steady dim white
    LED_SLOT_RADIO_FAILED, // the Thread radio (H2) doesn't answer: red, just before the S3 restarts
} led_slot_state_t;

typedef struct {
    uint8_t r, g, b;
} led_rgb_t;

// Brightest channel value any pattern emits (about 25% of 255).
#define LED_PATTERN_MAX_LEVEL 64

#define LED_PATTERN_PULSE_PERIOD_MS 3000
#define LED_PATTERN_BLINK_PERIOD_MS 2000
#define LED_PATTERN_BLINK_ON_MS     300
// The panel is 8 x 8. One ring grows from the centre to the corners and fades out once per period.
#define LED_PATTERN_PANEL_SIZE      8
#define LED_PATTERN_RING_PERIOD_MS  2200

// LED_SLOT_STARTING returns off here: the startup animation covers the whole panel, see below.
led_rgb_t led_pattern_color(led_slot_state_t state, uint32_t now_ms);

// Startup animation, a dim white ring breathing out from the centre. `x` is the column and `y` the row (0-7).
// The ring is symmetric about both centre lines, so it looks the same on panels wired row by row or zigzag.
led_rgb_t led_pattern_startup(int x, int y, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
