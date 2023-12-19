#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <led-updater.h>
#include <udp-server.h>
#include <wifi-initializer.h>

static const char* TAG = "app_main";

static void event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void app_main(void)
{
    ESP_LOGI(TAG, "creating default event loop.");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "initializing netif.");
    ESP_ERROR_CHECK(esp_netif_init());

    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event_callback, NULL);

    TaskHandle_t wifi_initializer_task_handle;
    xTaskCreate(wifi_initializer_task, "wifi_initializer_task", 4096, NULL, 1, &wifi_initializer_task_handle);
}

static void event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "(IP_EVENT_STA_GOT_IP) Got IP.");
        TaskHandle_t server_task_handle;
        xTaskCreate(server_task, "server_task", 4096, NULL, 1, &server_task_handle);

        TaskHandle_t led_updater_task_handle;
        led_updater_params_t* led_updater_params = malloc(sizeof(led_updater_params_t));
        led_updater_params->led_count = 100;
        led_updater_params->queue_size = 50;
        xTaskCreate(led_updater_task, "led_updater_task", 4096, led_updater_params, 2, &led_updater_task_handle);
        break;
    default:
        ESP_LOGI(TAG, "Generic IP_EVENT: %i", (int)event_id);
        break;
    }
}
