//
// Created by pxanych on 18.12.2023.
//

#include "wifi-initializer.h"


#include "freertos/FreeRTOS.h"
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <freertos/task.h>

#include "secrets/wifi_config.h"

static const char* TAG = "wifi_initializer";

static void event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void shutdown_handler();

void wifi_initializer_task(void* pvArgs)
{
    ESP_LOGI(TAG, "task started");

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_callback, NULL);
    esp_register_shutdown_handler(shutdown_handler);

    ESP_LOGI(TAG, "creating wifi interface.");
    esp_netif_create_default_wifi_sta();

    ESP_LOGI(TAG, "Configuring wifi.");
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

    ESP_LOGI(TAG, "Starting wifi.");
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelete(0);
}


static void event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != WIFI_EVENT)
    {
        return;
    }

    switch (event_id)
    {
    case WIFI_EVENT_STA_START: // wifi station started
        ESP_LOGI(TAG, "Connecting wifi.");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case WIFI_EVENT_STA_CONNECTED: // wifi station connected
        ESP_LOGI(TAG, "WiFi connected.");
        break;
    case WIFI_EVENT_STA_DISCONNECTED: // wifi station disconnected
        ESP_LOGI(TAG, "WiFi disconnected.");
        break;
    default:
        return;
    }
}

static void shutdown_handler()
{
    ESP_LOGI("shutdown_hadler", "Shutting down WiFi");
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}
