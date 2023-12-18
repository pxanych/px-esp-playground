//
// Created by pxanych on 17.12.2023.
//

#include "tasks/scan_aps.h"

#include <esp_event.h>

#include "freertos/FreeRTOS.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <FreeRTOSConfig.h>
#include <portmacro.h>
#include <freertos/projdefs.h>

#include <freertos/task.h>


void print_scan_results(const char* log_tag);
void wifi_event_monitor(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void scan_access_points_task(void* pvArgs)
{
    char* task_name = pcTaskGetName(NULL);

    esp_log_write(ESP_LOG_INFO, task_name, "task started\n");

    esp_log_write(ESP_LOG_INFO, task_name, "initializing netif...\n");
    ESP_ERROR_CHECK(esp_netif_init());
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    esp_log_write(ESP_LOG_INFO, task_name, "creating default event loop...\n");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");


    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_monitor, task_name, NULL);

    esp_log_write(ESP_LOG_INFO, task_name, "Configuring wifi...\n");
    const wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    esp_log_write(ESP_LOG_INFO, task_name, "Starting wifi...\n");
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    for (int i = 0; i < 5; i++)
    {
        esp_log_write(ESP_LOG_INFO, task_name, "Starting scan...\n");
        ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, false));
        esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(0);
}

void print_scan_results(const char* log_tag)
{
    uint16_t result_count;
    esp_wifi_scan_get_ap_num(&result_count);
    esp_log_write(ESP_LOG_INFO, log_tag, "Got %u scan results: \n", result_count);

    wifi_ap_record_t* results = malloc(result_count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&result_count, results);

    for (int i = 0; i < result_count; i++)
    {
        wifi_ap_record_t record = results[i];

        esp_log_write(ESP_LOG_INFO, log_tag,
                      "\t%i: %i - %02x:%02x:%02x:%02x:%02x:%02x - %s\n",
                      i,
                      record.rssi,
                      record.bssid[0],
                      record.bssid[1],
                      record.bssid[2],
                      record.bssid[3],
                      record.bssid[4],
                      record.bssid[5],
                      (char*)record.ssid
        );
    }

    esp_log_write(ESP_LOG_INFO, log_tag, "\n\n");
    free(results);
}

void wifi_event_monitor(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    const char* task_name = event_handler_arg;
    esp_log_write(ESP_LOG_INFO, task_name, "WIFI event: %lu\n", event_id);
    switch (event_id)
    {
    case WIFI_EVENT_SCAN_DONE:
        print_scan_results(task_name);
        break;
    default:
        break;
    }
}
