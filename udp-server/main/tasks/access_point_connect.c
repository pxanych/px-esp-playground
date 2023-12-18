//
// Created by pxanych on 18.12.2023.
//

#include "freertos/FreeRTOS.h"
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <freertos/task.h>

#include "tasks/access_point_connect.h"
#include <tasks/udp_server.h>
#include "secrets/wifi_config.h"

void event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void shutdown_handler();

void access_point_connect(void* pvArgs)
{
    char* task_name = pcTaskGetName(NULL);

    esp_log_write(ESP_LOG_INFO, task_name, "task started\n");

    // init netif
    esp_log_write(ESP_LOG_INFO, task_name, "initializing netif...\n");
    ESP_ERROR_CHECK(esp_netif_init());
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    // create event loop
    esp_log_write(ESP_LOG_INFO, task_name, "creating default event loop...\n");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, event_callback, task_name);
    esp_register_shutdown_handler(shutdown_handler);

    esp_log_write(ESP_LOG_INFO, task_name, "creating wifi interface...\n");
    esp_netif_t* wifi_netif = esp_netif_create_default_wifi_sta();
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    // config wifi
    esp_log_write(ESP_LOG_INFO, task_name, "Configuring wifi...\n");
    const wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));


    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_AP_NAME,
            .password = WIFI_AP_SECRET
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");

    // start wifi
    esp_log_write(ESP_LOG_INFO, task_name, "Starting wifi...\n");
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");


    vTaskDelete(0);
}


void event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    const char* task_name = event_handler_arg;

    if (event_base == WIFI_EVENT) // handle wifi events
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START: // wifi station started
            esp_log_write(ESP_LOG_INFO, task_name, "Connecting wifi...\n");
            ESP_ERROR_CHECK(esp_wifi_connect());
            esp_log_write(ESP_LOG_INFO, task_name, "Done.\n");
            break;
        case WIFI_EVENT_STA_CONNECTED: // wifi station connected
            esp_log_write(ESP_LOG_INFO, task_name, "WiFi connected.\n");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: // wifi station disconnected
            esp_log_write(ESP_LOG_INFO, task_name, "WiFi disconnected.\n");
            break;
        default:
            return;
        }
    }
    else if (event_base == IP_EVENT) // handle ip event
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            esp_log_write(ESP_LOG_INFO, task_name, "Got IP.\n");
            TaskHandle_t server_task_handle;
            xTaskCreate(server_task, "server_task", 4096, NULL, 1, &server_task_handle);
        }
    }
}

void shutdown_handler()
{
    ESP_LOGI("shutdown_hadler", "Shutting down WiFi");
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}
