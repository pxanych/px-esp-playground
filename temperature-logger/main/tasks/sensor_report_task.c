//
// Created by pxany on 18.10.2024.
//


#include <freertos/FreeRTOS.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_tls_errors.h>
#include <esp_tls.h>
#include <sys/param.h>
#include "secrets/secrets.h"
#include "bmp280.h"


extern const char isrg_root_x1_start[] asm("_binary_isrg_root_x1_pem_start");

esp_err_t http_event_handler(esp_http_client_event_t *evt);

void sensor_report_task(void *pvArgs) {

    bmp280_device *device = pvArgs;

    char *log_tag = "sensor_report_task";

    esp_http_client_config_t config = {
            .host = API_DOMAIN,
            .path = API_PATH,
            .buffer_size = 2048,
            .buffer_size_tx = 2048,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .event_handler = http_event_handler,
            .cert_pem = isrg_root_x1_start
    };
    ESP_LOGI(log_tag, "initializing HTTP client");
    esp_http_client_handle_t http_client = esp_http_client_init(&config);


    esp_http_client_set_method(http_client, HTTP_METHOD_POST);

    char post_data[256];
    while (1) {
        bmp280_read_data(device);
        uint32_t pressure_x100 = device->measure_data.pa_x256 * 100 / 256;
        int32_t temperature_x100 = device->measure_data.deg_c_x100;

        sprintf(post_data, "{\"temperature\": %li.%02li, \"pressure\": %li.%02li}",
                temperature_x100 / 100, temperature_x100 % 100,
                pressure_x100 / 100, pressure_x100 % 100
        );
        esp_http_client_set_post_field(http_client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(http_client);
        if (err == ESP_OK) {
            ESP_LOGI(log_tag, "HTTPS Status = %d, content_length = %lli",
                     esp_http_client_get_status_code(http_client), esp_http_client_get_content_length(http_client));
        } else {
            ESP_LOGE(log_tag, "Error perform http request %s", esp_err_to_name(err));
        }

        vTaskDelay(10 * xPortGetTickRateHz()); // 10 seconds
    }
}


esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    char * log_tag = "http_event_handler";
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(log_tag, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(log_tag, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(log_tag, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(log_tag, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(log_tag, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(log_tag, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(log_tag, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t) evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(log_tag, "Last esp error code: 0x%x", err);
                ESP_LOGI(log_tag, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(log_tag, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}