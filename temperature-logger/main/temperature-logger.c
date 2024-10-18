#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <bmp280.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_wifi_default.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "secrets/secrets.h"
#include "sensor_report_task.h"


static void wifi_event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static char *TAG = "app_main";
static bmp280_device device;
static TaskHandle_t sensor_report_task_handle;
static StaticSemaphore_t sensor_report_task_semaphore;
static SemaphoreHandle_t sensor_report_task_semaphore_handle;

void app_main(void) {
    ESP_LOGI(TAG, "initializing i2c master bus");
    i2c_master_bus_handle_t masterBusHandle;
    i2c_master_bus_config_t masterBusConfig = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1, // autoselect
            .scl_io_num = GPIO_NUM_4,
            .sda_io_num = GPIO_NUM_5,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
    };
    sensor_report_task_semaphore_handle = xSemaphoreCreateRecursiveMutexStatic(&sensor_report_task_semaphore);
    ESP_ERROR_CHECK(i2c_new_master_bus(&masterBusConfig, &masterBusHandle));

    ESP_LOGI(TAG, "initializing bmp280 device");
    ESP_ERROR_CHECK(bmp280_init(masterBusHandle, BMP280_ADDR_0, &device));

    bmp280_ctrl_meas ctrl_meas = {
            .bits.osrs_t = 2,   // x2  oversampling
            .bits.osrs_p = 5,   // x16 oversampling
            .bits.mode = 3,      // normal mode
    };
    bmp280_config_t bmp_config = {
            .bits.t_sb = 4,      // measure period 500ms
            .bits.filter = 5,    // filter x16
            .bits.spi3w_en = 0,
    };
    ESP_LOGI(TAG, "setting bmp280 device config");
    ESP_ERROR_CHECK(bmp280_set_config(&device, ctrl_meas, bmp_config));

    ESP_LOGI(TAG, "creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_callback, NULL);
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_callback, NULL);

    ESP_LOGI(TAG, "initializing NVS storage");
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "initializing netif");
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_LOGI(TAG, "creating wifi interface");
    esp_netif_create_default_wifi_sta();

    ESP_LOGI(TAG, "configuring wifi");
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
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

    ESP_LOGI(TAG, "starting wifi");
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelay(1000);
}


static void stop_sensor_report_task() {
    char * log_tag = "stop_sensor_report_task";
    ESP_LOGI(log_tag, "taking semaphore");
    xSemaphoreTakeRecursive(sensor_report_task_semaphore_handle, portMAX_DELAY);
    if (sensor_report_task_handle != NULL) {
        ESP_LOGI(log_tag, "will delete task");
        vTaskDelete(sensor_report_task_handle);
        sensor_report_task_handle = NULL;
        ESP_LOGI(log_tag, "task deleted");
    } else {
        ESP_LOGI(log_tag, "already stopped");
    }
    ESP_LOGI(log_tag, "releasing semaphore");
    xSemaphoreGiveRecursive(sensor_report_task_semaphore_handle);
}

static void start_sensor_report_task() {
    char * log_tag = "start_sensor_report_task";
    ESP_LOGI(log_tag, "taking semaphore");
    xSemaphoreTakeRecursive(sensor_report_task_semaphore_handle, portMAX_DELAY);
    ESP_LOGI(log_tag, "will create task");
    stop_sensor_report_task();
    ESP_LOGI(log_tag, "task created");
    xTaskCreate(sensor_report_task, "sensor_report_task", CONFIG_ESP_MAIN_TASK_STACK_SIZE,
                &device, 1, &sensor_report_task_handle);
    ESP_LOGI(log_tag, "releasing semaphore");
    xSemaphoreGiveRecursive(sensor_report_task_semaphore_handle);
}

static void wifi_event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != WIFI_EVENT) {
        return;
    }
    char *log_tag = "ip_event_callback";
    switch (event_id) {
        case WIFI_EVENT_STA_START: // wifi station started
            ESP_LOGI(log_tag, "Connecting wifi.");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case WIFI_EVENT_STA_CONNECTED: // wifi station connected
            ESP_LOGI(log_tag, "WiFi connected.");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: // wifi station disconnected
            ESP_LOGI(log_tag, "WiFi disconnected.");
            break;
        default:
            return;
    }
}

static void ip_event_callback(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != IP_EVENT) {
        return;
    }

    char *log_tag = "ip_event_callback";
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(log_tag, "(IP_EVENT_STA_GOT_IP) Got IP.");
            start_sensor_report_task();
            break;
        case IP_EVENT_STA_LOST_IP:
            stop_sensor_report_task();
            break;
        default:
            ESP_LOGI(log_tag, "Generic IP_EVENT: %i", (int)event_id);
            break;
    }
}
