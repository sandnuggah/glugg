#pragma once

// Maps a slot's state and the current time to an LED colour.
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
    LED_SLOT_STARTING,     // network not up yet: a dim white dot sweeping back and forth across the stick
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
// One sweep from LED 1 to LED 8 and back.
#define LED_PATTERN_SWEEP_PERIOD_MS 1400

// `slot` (0-7) is only used by the sweep, which spans the whole stick.
led_rgb_t led_pattern_color(led_slot_state_t state, int slot, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
