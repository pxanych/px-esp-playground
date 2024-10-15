#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <sensor_init.h>

void app_main(void) {
    TaskHandle_t sensor_init_task_handle;
    xTaskCreate(
            sensor_init_task,
            "sensor_init_task",
            4096,
            NULL,
            1,
            &sensor_init_task_handle
    );
}
