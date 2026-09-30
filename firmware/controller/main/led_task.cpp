#include "led_task.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>

#include "app_state.h"
#include "led_pattern.h"

static const char *TAG = "led_task";

#define LED_GPIO       13
#define FRAME_MS       20
#define TASK_STACK     3072
#define TASK_PRIORITY  2

static led_strip_handle_t s_strip;

static void led_task(void *)
{
    led_slot_state_t states[SLOT_COUNT];
    for (;;) {
        uint32_t now;
        {
            AppStateLock m;
            now = app_now_ms(); // under the lock, so no event can be newer than `now`
            for (int i = 0; i < SLOT_COUNT; i++) {
                states[i] = sensor_mgr_led_state(m.get(), i, now);
            }
        }
        for (int i = 0; i < SLOT_COUNT; i++) {
            led_rgb_t c = led_pattern_color(states[i], i, now);
            led_strip_set_pixel(s_strip, i, c.r, c.g, c.b);
        }
        led_strip_refresh(s_strip);
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
    }
}

void led_task_start()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = SLOT_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    // With DMA the whole frame (8 x 24 bits) is in one buffer, so no refill interrupt runs mid-frame; a refill held
    // off by a flash write (NVS save) would garble the LEDs.
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 256,
        .flags = {.with_dma = true},
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));
    led_strip_clear(s_strip);
    if (xTaskCreate(led_task, "leds", TASK_STACK, nullptr, TASK_PRIORITY, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "Couldn't start the LED task");
    }
}
