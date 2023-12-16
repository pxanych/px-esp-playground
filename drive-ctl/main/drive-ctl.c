#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <driver/gptimer_types.h>
#include <esp_private/panic_internal.h>
#include <freertos/queue.h>

#include "../../esp-idf/components/esp_driver_gptimer/src/gptimer_priv.h"

bool timerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
void timerHandlerTask(void* args);

typedef struct {
    QueueHandle_t queue_handle;
    gptimer_handle_t timer_handle;
} MyCtx;

/**
* Based on GPIO and GPTimer ESP32 APIs
* Also uses FreeRTOS queues and tasks
*/
void app_main(void) {
    QueueHandle_t queueHandle = xQueueCreate(10, sizeof(gptimer_alarm_event_data_t));

    const uint32_t timerResolution = 1000 * 1000;

    gpio_config_t gpioConfig = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1 << GPIO_NUM_4) | (1 << GPIO_NUM_5) | (1 << GPIO_NUM_6),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE
    };
    ESP_ERROR_CHECK(gpio_config(&gpioConfig));


    gptimer_config_t timerCfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = timerResolution,
    };
    gptimer_handle_t timerHandle;
    ESP_ERROR_CHECK(gptimer_new_timer(&timerCfg, &timerHandle));

    gptimer_alarm_config_t alarmCfg = {
        .alarm_count = 500000,
        .reload_count = 0,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timerHandle, &alarmCfg));

    gptimer_event_callbacks_t callbacks = { .on_alarm = timerCallback };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timerHandle, &callbacks, queueHandle));

    TaskHandle_t taskHandle;
    MyCtx* pCtx = malloc(sizeof(MyCtx));
    pCtx->queue_handle = queueHandle;
    pCtx->timer_handle = timerHandle;
    if (!xTaskCreate(timerHandlerTask, "timer handler task", 4096, pCtx, 1, &taskHandle))
    {
        panic_abort("failed to create task");
    }

    ESP_ERROR_CHECK(gptimer_enable(timerHandle));
    ESP_ERROR_CHECK(gptimer_start(timerHandle));
}

bool timerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    QueueHandle_t queue_handle = user_ctx;
    BaseType_t highPrioAwoken;
    xQueueSendToBackFromISR(queue_handle, edata, &highPrioAwoken);
    return highPrioAwoken == pdTRUE;
}


/**
* This task updates GPIO pin states to spin-up BLDC motor.
* It also schedules next update.
*/
void timerHandlerTask(void* args)
{
    MyCtx* pCtx = args;
    gptimer_handle_t gptimer_handle = pCtx->timer_handle;
    QueueHandle_t queue_handle = pCtx->queue_handle;
    int timer_freq = gptimer_handle->resolution_hz;

    int ns_till_next_switch = timer_freq * 1000 / 1;

    int bitToSet = 0;
    // 0 - gpio4
    // 0 - gpio5
    // 0 - gpio6

    int tf = 400;
    int target_freq = tf;
    int accel_base = 8;
    int accel = accel_base;


    gptimer_alarm_event_data_t alarm_event_data;
    for(;;)
    {
        if (!xQueueReceive(queue_handle, &alarm_event_data, 100))
        {
            continue;
        }
        int freq = timer_freq * 1000 / ns_till_next_switch;

        if (accel > 0 && freq >= target_freq)
        {
            target_freq = 20;
            accel = 0;
        }

        if (accel < 0 && freq <= target_freq)
        {
            target_freq = tf;
            accel = accel_base;
        }

        if (freq < 10)
        {
            ns_till_next_switch = ns_till_next_switch / 100 * 95;
        }

        int tgt_ns = 1000000000 / (freq + accel);
        int dns = (tgt_ns - ns_till_next_switch) / freq;
        ns_till_next_switch = ns_till_next_switch + dns;

        gptimer_alarm_config_t alarmCfg = { .alarm_count = alarm_event_data.alarm_value + ns_till_next_switch / 1000 };
        ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer_handle, &alarmCfg));

        // do switch
        gpio_set_level(GPIO_NUM_4, 0);
        gpio_set_level(GPIO_NUM_5, 0);
        gpio_set_level(GPIO_NUM_6, 0);
        switch (bitToSet)
        {
        case 0:
            gpio_set_level(GPIO_NUM_4, 1);
            break;
        case 1:
            gpio_set_level(GPIO_NUM_5, 1);
            break;
        case 2:
            gpio_set_level(GPIO_NUM_6, 1);
            break;
        default:
            break;
        }

        bitToSet = (bitToSet + 1) % 3;
    }
}
