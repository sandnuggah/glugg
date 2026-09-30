// Phase 0 LED test: checks the 8-LED stick wiring (level shifter, colour order, pixel order)
// and shows every slot pattern the real firmware will use. Loops forever; watch the console.

#include <inttypes.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_pattern.h"
#include "led_strip.h"

#define NUM_LEDS    CONFIG_LEDTEST_STRIP_LEDS
#define FRAME_MS    20
#define TEST_LEVEL  LED_PATTERN_MAX_LEVEL

#ifdef CONFIG_LEDTEST_ONBOARD_RGB_ORDER
#define ONBOARD_RGB_ORDER true
#else
#define ONBOARD_RGB_ORDER false
#endif

static const char *TAG = "led_test";

static led_strip_handle_t s_stick;
static led_strip_handle_t s_onboard;

static led_strip_handle_t new_strip(int gpio, uint32_t leds, bool rgb_order)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = leds,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = rgb_order ? LED_STRIP_COLOR_COMPONENT_FMT_RGB : LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    led_strip_handle_t strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    return strip;
}

// LED 1 of the stick (pixel 0, DIN end) is mirrored on the onboard LED, so the code can be checked before the stick is wired.
static void show(const led_rgb_t *px)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_stick, i, px[i].r, px[i].g, px[i].b));
    }
    ESP_ERROR_CHECK(led_strip_set_pixel(s_onboard, 0, px[0].r, px[0].g, px[0].b));
    ESP_ERROR_CHECK(led_strip_refresh(s_stick));
    ESP_ERROR_CHECK(led_strip_refresh(s_onboard));
}

static void fill(led_rgb_t c, uint32_t hold_ms)
{
    led_rgb_t px[NUM_LEDS];
    for (int i = 0; i < NUM_LEDS; i++) {
        px[i] = c;
    }
    show(px);
    vTaskDelay(pdMS_TO_TICKS(hold_ms));
}

static void colour_check(void)
{
    ESP_LOGI(TAG, "Colour check: all LEDs RED");
    fill((led_rgb_t){TEST_LEVEL, 0, 0}, 1500);
    ESP_LOGI(TAG, "Colour check: all LEDs GREEN");
    fill((led_rgb_t){0, TEST_LEVEL, 0}, 1500);
    ESP_LOGI(TAG, "Colour check: all LEDs BLUE");
    fill((led_rgb_t){0, 0, TEST_LEVEL}, 1500);
}

static void chase(void)
{
    ESP_LOGI(TAG, "Pixel order: one white LED walks from DIN end (LED 1) to LED %d", NUM_LEDS);
    for (int lit = 0; lit < NUM_LEDS; lit++) {
        led_rgb_t px[NUM_LEDS] = {0};
        px[lit] = (led_rgb_t){TEST_LEVEL / 2, TEST_LEVEL / 2, TEST_LEVEL / 2};
        show(px);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

static void run_patterns(const led_slot_state_t *slots, size_t count, uint32_t duration_ms)
{
    int64_t start_us = esp_timer_get_time();
    while ((esp_timer_get_time() - start_us) / 1000 < duration_ms) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        led_rgb_t px[NUM_LEDS];
        for (int i = 0; i < NUM_LEDS; i++) {
            px[i] = led_pattern_color(slots[i % count], i, now_ms);
        }
        show(px);
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
    }
}

static void startup_sweep(uint32_t duration_ms)
{
    static const led_slot_state_t k_starting[] = {LED_SLOT_STARTING};
    ESP_LOGI(TAG, "Startup sweep for %" PRIu32 " s: a dim white dot goes back and forth across all LEDs",
             duration_ms / 1000);
    run_patterns(k_starting, 1, duration_ms);
}

static void slot_patterns(uint32_t duration_ms)
{
    static const led_slot_state_t k_slots[] = {
        LED_SLOT_OPEN,    LED_SLOT_CLOSED, LED_SLOT_PAIRING, LED_SLOT_UNREACHABLE,
        LED_SLOT_WAITING, LED_SLOT_EMPTY,  LED_SLOT_PAIRING, LED_SLOT_UNREACHABLE,
    };
    ESP_LOGI(TAG, "Slot patterns for %" PRIu32 " s: LED 1 open (amber) | 2,6 closed/empty (off) | "
             "3,7 pairing (green pulse) | 4,8 not heard from (blue blink) | 5 waiting for first report (dim white)",
             duration_ms / 1000);
    run_patterns(k_slots, sizeof(k_slots) / sizeof(k_slots[0]), duration_ms);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Stick: %d LEDs on GPIO%d. Onboard LED on GPIO%d mirrors LED 1.",
             NUM_LEDS, CONFIG_LEDTEST_STRIP_GPIO, CONFIG_LEDTEST_ONBOARD_GPIO);
    s_stick = new_strip(CONFIG_LEDTEST_STRIP_GPIO, NUM_LEDS, false);
    s_onboard = new_strip(CONFIG_LEDTEST_ONBOARD_GPIO, 1, ONBOARD_RGB_ORDER);

    for (;;) {
        colour_check();
        chase();
        startup_sweep(6000);
        slot_patterns(15000);
        fill((led_rgb_t){0}, 1000);
    }
}
