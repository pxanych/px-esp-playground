//
// Created by pxanych on 18.12.2023.
//

#include "freertos/FreeRTOS.h"
#include "led-updater.h"

#include <esp_log.h>
#include <led_strip_interface.h>
#include <led_strip_rmt.h>
#include <led_strip_types.h>
#include <string.h>
#include <driver/gptimer.h>
#include <freertos/queue.h>
#include <soc/gpio_num.h>

static const char* TAG = "led_updater_task";
static led_updater_params_t TASK_PARAMETERS;
static QueueHandle_t QUEUE_HANDLE;
static size_t QUEUE_ITEM_SIZE;
static led_strip_handle_t STRIP_HANDLE;
static gptimer_handle_t TIMER_HANDLE;
static EventGroupHandle_t EVENT_GROUP_HANDLE;
static int LED_UPDATE_NS_DURATION = 1000 * 1000 * 1000 / 30000; // ~33k 24-bit pixels @ 800kbit/s

static bool alarm_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx);
static void do_update(led_color* leds);

void led_updater_task(void* pvArgs)
{
    TASK_PARAMETERS = *(led_updater_params_t*)pvArgs;
    free(pvArgs);

    // init incoming queue
    QUEUE_ITEM_SIZE = sizeof(led_color) * TASK_PARAMETERS.led_count;
    ESP_LOGI(TAG, "Will create queue for %i items of %i bytes", TASK_PARAMETERS.queue_size, QUEUE_ITEM_SIZE);
    QUEUE_HANDLE = xQueueCreate(TASK_PARAMETERS.queue_size, QUEUE_ITEM_SIZE);
    //////////////

    // init led strip driver
    led_strip_config_t stripConfig = {
        .led_model = LED_MODEL_WS2812,
        .strip_gpio_num = GPIO_NUM_48,
        .max_leds = TASK_PARAMETERS.led_count,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags.invert_out = false
    };
    led_strip_rmt_config_t stripRmtConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10 Mhz
        .flags.with_dma = true
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&stripConfig, &stripRmtConfig, &STRIP_HANDLE));
    ///////////////////////

    // init timer & alert
    int us_per_update = LED_UPDATE_NS_DURATION / (1000 * TASK_PARAMETERS.led_count);
    if (us_per_update < 20000) // 50 ups
    {
        us_per_update = 20000;
    }
    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000 * 1000, // 1 MHz
    };
    gptimer_event_callbacks_t callbacks = {
        .on_alarm = alarm_handler
    };
    gptimer_alarm_config_t alarm_cfg = {
        .reload_count = 0,
        .alarm_count = us_per_update,
        .flags.auto_reload_on_alarm = 1
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &TIMER_HANDLE));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(TIMER_HANDLE, &callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(TIMER_HANDLE, &alarm_cfg));
    ESP_ERROR_CHECK(gptimer_enable(TIMER_HANDLE));
    ESP_ERROR_CHECK(gptimer_start(TIMER_HANDLE));
    /////////////

    // init event group
    EVENT_GROUP_HANDLE = xEventGroupCreate();
    ///////////////////

    led_color* leds = malloc(QUEUE_ITEM_SIZE);
    ESP_LOGI(TAG, "Starting led_updateer main loop");
    for (;;)
    {
        while (!xQueueReceive(QUEUE_HANDLE, leds, pdMS_TO_TICKS(1000)))
        {

        }
        while (!(BIT0 & xEventGroupWaitBits(EVENT_GROUP_HANDLE, BIT0, pdTRUE, pdTRUE, 100)))
        {

        }
        do_update(leds);
    }
    free(leds);
    vTaskDelete(NULL);
}

void submit_update(uint16_t len, led_color* leds)
{
    uint8_t* new_queue_item = malloc(QUEUE_ITEM_SIZE);
    memcpy(new_queue_item, leds, len * sizeof(led_color));
    if (len < TASK_PARAMETERS.led_count)
    {
        memset(
            new_queue_item + len * sizeof(led_color),
            0,
            (TASK_PARAMETERS.led_count - len) * sizeof(led_color)
        );
    }

    if (xQueueSendToBack(QUEUE_HANDLE, new_queue_item, 0) == errQUEUE_FULL)
    {
        void* to_ignore = malloc(QUEUE_ITEM_SIZE);
        xQueueReceive(QUEUE_HANDLE, to_ignore, 0);
        free(to_ignore);
        if (xQueueSendToBack(QUEUE_HANDLE, new_queue_item, 0) == errQUEUE_FULL)
        {
            ESP_LOGW(TAG, "Skipped update as queue is full (also failed to free any space in queue)");
        }
    }
    free(new_queue_item);
}

bool alarm_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx)
{
    BaseType_t ret_value;
    xEventGroupSetBitsFromISR(EVENT_GROUP_HANDLE, BIT0, &ret_value);
    return ret_value == pdTRUE;
}

void do_update(led_color* leds)
{
    for (int i = 0; i < TASK_PARAMETERS.led_count; i++)
    {
        led_color color = leds[i];
        STRIP_HANDLE->set_pixel(STRIP_HANDLE, i, color.red, color.green, color.blue);
    }
    STRIP_HANDLE->refresh(STRIP_HANDLE);
}
