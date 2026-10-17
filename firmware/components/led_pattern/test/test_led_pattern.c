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
        CHECK(is_off(led_pattern_color(LED_SLOT_EMPTY, t)));
        CHECK(is_off(led_pattern_color(LED_SLOT_CLOSED, t)));

        // Warm amber at about 25%: exact levels, so a change is deliberate.
        led_rgb_t open = led_pattern_color(LED_SLOT_OPEN, t);
        CHECK(open.r == LED_PATTERN_MAX_LEVEL && open.g == 25 && open.b == 0);

        led_rgb_t pairing = led_pattern_color(LED_SLOT_PAIRING, t);
        CHECK(pairing.r == 0 && pairing.b == 0 && pairing.g <= LED_PATTERN_MAX_LEVEL);

        led_rgb_t unreachable = led_pattern_color(LED_SLOT_UNREACHABLE, t);
        CHECK(unreachable.r == 0 && unreachable.g == 0 && (unreachable.b == 0 || unreachable.b == 16));

        led_rgb_t waiting = led_pattern_color(LED_SLOT_WAITING, t);
        CHECK(is_grey(waiting) && waiting.r == 8);

        led_rgb_t failed = led_pattern_color(LED_SLOT_RADIO_FAILED, t);
        CHECK(failed.r == 48 && failed.g == 0 && failed.b == 0);
    }

    // Pulse: dark at the start of a period, brightest at the middle.
    CHECK(led_pattern_color(LED_SLOT_PAIRING, 0).g == 0);
    CHECK(led_pattern_color(LED_SLOT_PAIRING, LED_PATTERN_PULSE_PERIOD_MS / 2).g == LED_PATTERN_MAX_LEVEL);
    CHECK(led_pattern_color(LED_SLOT_PAIRING, LED_PATTERN_PULSE_PERIOD_MS).g == 0);

    // Blink: on at the start of each period, off after the on-time.
    CHECK(led_pattern_color(LED_SLOT_UNREACHABLE, 0).b > 0);
    CHECK(led_pattern_color(LED_SLOT_UNREACHABLE, LED_PATTERN_BLINK_ON_MS).b == 0);
    CHECK(led_pattern_color(LED_SLOT_UNREACHABLE, LED_PATTERN_BLINK_PERIOD_MS).b > 0);

    // uint32 millisecond counter wraps after ~49 days; must not crash or go out of range.
    CHECK(led_pattern_color(LED_SLOT_PAIRING, UINT32_MAX).g <= LED_PATTERN_MAX_LEVEL);
    CHECK(is_off(led_pattern_color(LED_SLOT_STARTING, UINT32_MAX)));
}

static led_rgb_t ring(int x, int y, uint32_t t) { return led_pattern_startup(x, y, t); }

static void test_startup(void)
{
    const uint32_t period = LED_PATTERN_RING_PERIOD_MS;
    const int last = LED_PATTERN_PANEL_SIZE - 1;

    // Dark at the start of each period, then the ring appears around the four middle LEDs.
    for (int y = 0; y <= last; y++) {
        for (int x = 0; x <= last; x++) {
            CHECK(is_off(ring(x, y, 0)));
            CHECK(is_off(ring(x, y, period)));
        }
    }
    CHECK(max_channel(ring(3, 3, period / 8)) == 19);
    CHECK(max_channel(ring(4, 4, period / 8)) == 19);
    CHECK(max_channel(ring(2, 3, period / 8)) == 2); // the ring's soft edge
    CHECK(is_off(ring(1, 3, period / 8)));
    CHECK(is_off(ring(0, 0, period / 8)));
    // Late in the period it has reached the corners, faded, and left the middle.
    CHECK(max_channel(ring(0, 0, period * 7 / 8)) == 2);
    CHECK(is_off(ring(3, 3, period * 7 / 8)));

    // Every frame: grey, dim, symmetric about both centre lines (so zigzag panels look the same), and every LED
    // gets lit at some point; the same frame repeats one period later.
    int ever_lit[LED_PATTERN_PANEL_SIZE][LED_PATTERN_PANEL_SIZE] = {{0}};
    for (uint32_t t = 0; t < 2 * period; t += 20) {
        for (int y = 0; y <= last; y++) {
            for (int x = 0; x <= last; x++) {
                led_rgb_t c = ring(x, y, t);
                CHECK(is_grey(c) && c.r <= 20);
                CHECK(c.r == ring(last - x, y, t).r && c.r == ring(x, last - y, t).r && c.r == ring(y, x, t).r);
                CHECK(c.r == ring(x, y, t + period).r);
                ever_lit[y][x] |= !is_off(c);
            }
        }
    }
    for (int y = 0; y <= last; y++) {
        for (int x = 0; x <= last; x++) {
            CHECK(ever_lit[y][x]);
        }
    }

    // Off the panel stays dark, and the uint32 millisecond counter can wrap.
    CHECK(is_off(ring(-1, 3, period / 8)) && is_off(ring(3, 8, period / 8)) && is_off(ring(8, 8, period / 2)));
    CHECK(max_channel(ring(0, 0, UINT32_MAX)) <= LED_PATTERN_MAX_LEVEL);
}

int main(void)
{
    test_slot_patterns();
    test_startup();
    if (s_failures) {
        printf("led_pattern: %d checks failed\n", s_failures);
        return 1;
    }
    printf("led_pattern: all tests passed\n");
    return 0;
}
