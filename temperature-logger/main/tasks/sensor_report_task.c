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

static const char HEX_DIGITS[] = "0123456789ABCDEF";

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

    const int post_buffer_size = 256;
    char *post_data = malloc(post_buffer_size);
    char *auth_header = malloc(65);
    char *hmac_key = HMAC_KEY;
    uint32_t hmac_key_length = strlen(hmac_key);
    auth_header[64] = 0;
    while (1) {
        bmp280_read_data(device);
        float pressure_x256 = (float)device->measure_data.pa_x256;
        float temperature_x100 = (float)device->measure_data.deg_c_x100;

        memset(post_data, 0, post_buffer_size);
        sprintf(post_data, "{\"temperature\": %f, \"pressure\": %f, \"node\": \"%s\"}",
                temperature_x100 / 100, pressure_x256 / 256, NODE_ID);
        uint32_t post_data_size = strlen(post_data);

        mbedtls_sha256_context sha256_ctx;
        mbedtls_sha256_init(&sha256_ctx);
        mbedtls_sha256_starts(&sha256_ctx, false);
        mbedtls_sha256_update(&sha256_ctx, (uint8_t *)post_data, post_data_size);
        mbedtls_sha256_update(&sha256_ctx, (uint8_t *)hmac_key, hmac_key_length);
        mbedtls_sha256_finish(&sha256_ctx, (uint8_t *)auth_header);
        mbedtls_sha256_free(&sha256_ctx);

        int i = 32;
        while (i > 0) {
            uint8_t byte = auth_header[i - 1];
            auth_header[i * 2 - 1] = HEX_DIGITS[(byte >> 4) & 0x0f];
            auth_header[i * 2 - 2] = HEX_DIGITS[byte & 0x0f];
            i--;
        }

        esp_http_client_set_header(http_client, "Authorization", auth_header);
        esp_http_client_set_post_field(http_client, post_data, (int32_t)post_data_size);

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