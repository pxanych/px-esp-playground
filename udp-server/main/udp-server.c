#include <stdio.h>
#include <esp_wifi.h>
#include <tasks/access_point_connect.h>
#include <tasks/scan_aps.h>

void app_main(void)
{
    TaskHandle_t task_handle;
    // xTaskCreate(scan_access_points_task, "scan_access_points", 4096, NULL, 0, &task_handle);
    xTaskCreate(access_point_connect, "connect_AP", 4096, NULL, 0, &task_handle);
    vTaskDelete(NULL);
}
