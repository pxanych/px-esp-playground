#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <stdio.h>
#include <driver/gpio.h>
#include <hal/gpio_hal.h>
#include <led_strip.h>
#include <led_strip_interface.h>

void app_main(void) {
    led_strip_handle_t stripHandle;

    led_strip_config_t stripConfig = {
        .led_model = LED_MODEL_WS2812,
        .strip_gpio_num = GPIO_NUM_4,
        .max_leds = 100,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags.invert_out = false
    };

    led_strip_rmt_config_t stripRmtConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .flags.with_dma = true
    };

    esp_err_t ledInitResult = led_strip_new_rmt_device(&stripConfig, &stripRmtConfig, &stripHandle);
    ESP_ERROR_CHECK(ledInitResult);

    int i = 0, j = 0;
    int r,g,b;
    while (true) {
        for (j = 0; j < 100; j++) {
            int k = i + j;
            r = (k + 33) % 100;
            g = (k + 66) % 100;
            b = k % 100;

            r = r - 50;
            g = g - 50;
            b = b - 50;

            r = abs(r);
            g = abs(g);
            b = abs(b);

            r = r - 25;
            g = g - 25;
            b = b - 25;

            r = r < 0 ? 0 : r;
            g = g < 0 ? 0 : g;
            b = b < 0 ? 0 : b;

            r = r * 5;
            g = g * 5;
            b = b * 5;
            stripHandle->set_pixel(stripHandle, j, r, g, b);
        }
        i = (i + 1) % 100;
        stripHandle->refresh(stripHandle);
        // vTaskDelay(3);
    }

}
