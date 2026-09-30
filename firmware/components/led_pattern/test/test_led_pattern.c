// Host unit test: `make -C firmware/components/led_pattern/test`
#include <stdio.h>
#include <stdlib.h>

#include "led_pattern.h"

static int s_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            s_failures++;                                                    \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

static int is_off(led_rgb_t c) { return c.r == 0 && c.g == 0 && c.b == 0; }
static int is_grey(led_rgb_t c) { return c.r == c.g && c.g == c.b; }
static int max_channel(led_rgb_t c) { return c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b); }

static void test_slot_patterns(void)
{
    for (uint32_t t = 0; t < 10000; t += 7) {
        int slot = (int)(t % 8);
        CHECK(is_off(led_pattern_color(LED_SLOT_EMPTY, slot, t)));
        CHECK(is_off(led_pattern_color(LED_SLOT_CLOSED, slot, t)));

        // Warm amber at about 25%: exact levels, so a change is deliberate.
        led_rgb_t open = led_pattern_color(LED_SLOT_OPEN, slot, t);
        CHECK(open.r == LED_PATTERN_MAX_LEVEL && open.g == 25 && open.b == 0);

        led_rgb_t pairing = led_pattern_color(LED_SLOT_PAIRING, slot, t);
        CHECK(pairing.r == 0 && pairing.b == 0 && pairing.g <= LED_PATTERN_MAX_LEVEL);

        led_rgb_t unreachable = led_pattern_color(LED_SLOT_UNREACHABLE, slot, t);
        CHECK(unreachable.r == 0 && unreachable.g == 0 && (unreachable.b == 0 || unreachable.b == 16));

        led_rgb_t waiting = led_pattern_color(LED_SLOT_WAITING, slot, t);
        CHECK(is_grey(waiting) && waiting.r == 8);

        led_rgb_t failed = led_pattern_color(LED_SLOT_RADIO_FAILED, slot, t);
        CHECK(failed.r == 48 && failed.g == 0 && failed.b == 0);
    }

    // Pulse: dark at the start of a period, brightest at the middle.
    CHECK(led_pattern_color(LED_SLOT_PAIRING, 0, 0).g == 0);
    CHECK(led_pattern_color(LED_SLOT_PAIRING, 0, LED_PATTERN_PULSE_PERIOD_MS / 2).g == LED_PATTERN_MAX_LEVEL);
    CHECK(led_pattern_color(LED_SLOT_PAIRING, 0, LED_PATTERN_PULSE_PERIOD_MS).g == 0);

    // Blink: on at the start of each period, off after the on-time.
    CHECK(led_pattern_color(LED_SLOT_UNREACHABLE, 0, 0).b > 0);
    CHECK(led_pattern_color(LED_SLOT_UNREACHABLE, 0, LED_PATTERN_BLINK_ON_MS).b == 0);
    CHECK(led_pattern_color(LED_SLOT_UNREACHABLE, 0, LED_PATTERN_BLINK_PERIOD_MS).b > 0);

    // uint32 millisecond counter wraps after ~49 days; must not crash or go out of range.
    CHECK(led_pattern_color(LED_SLOT_PAIRING, 0, UINT32_MAX).g <= LED_PATTERN_MAX_LEVEL);
    CHECK(max_channel(led_pattern_color(LED_SLOT_STARTING, 7, UINT32_MAX)) <= LED_PATTERN_MAX_LEVEL);
}

static void test_sweep(void)
{
    const uint32_t half = LED_PATTERN_SWEEP_PERIOD_MS / 2;
    // The dot starts on LED 0, reaches LED 7 half-way through the period and is back on LED 0 at the end.
    CHECK(max_channel(led_pattern_color(LED_SLOT_STARTING, 0, 0)) == 20);
    CHECK(is_off(led_pattern_color(LED_SLOT_STARTING, 7, 0)));
    CHECK(max_channel(led_pattern_color(LED_SLOT_STARTING, 7, half)) == 20);
    CHECK(is_off(led_pattern_color(LED_SLOT_STARTING, 0, half)));
    CHECK(max_channel(led_pattern_color(LED_SLOT_STARTING, 0, LED_PATTERN_SWEEP_PERIOD_MS)) == 20);

    // Every frame: grey, dim, at most three LEDs lit and they are neighbours; every LED gets its turn.
    int ever_lit[8] = {0};
    for (uint32_t t = 0; t < 3 * LED_PATTERN_SWEEP_PERIOD_MS; t += 20) {
        int lit = 0, first = -1, last = -1;
        for (int slot = 0; slot < 8; slot++) {
            led_rgb_t c = led_pattern_color(LED_SLOT_STARTING, slot, t);
            CHECK(is_grey(c) && c.r <= 20);
            if (!is_off(c)) {
                lit++;
                ever_lit[slot] = 1;
                first = first < 0 ? slot : first;
                last = slot;
            }
        }
        CHECK(lit >= 1 && lit <= 3 && last - first == lit - 1);
    }
    for (int slot = 0; slot < 8; slot++) {
        CHECK(ever_lit[slot]);
    }
    // Out-of-range slots stay dark rather than reading past the stick.
    CHECK(is_off(led_pattern_color(LED_SLOT_STARTING, -1, 0)));
    CHECK(is_off(led_pattern_color(LED_SLOT_STARTING, 8, half)));
}

int main(void)
{
    test_slot_patterns();
    test_sweep();
    if (s_failures) {
        printf("led_pattern: %d checks failed\n", s_failures);
        return 1;
    }
    printf("led_pattern: all tests passed\n");
    return 0;
}
