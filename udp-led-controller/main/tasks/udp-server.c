//
// Created by pxanych on 18.12.2023.
//

#include "udp-server.h"

#include <esp_log.h>
#include <led-updater.h>
#include <lwip/sockets.h>

static const char* TAG = "server_task";

void server_task(void* pvArgs)
{
    ESP_LOGI(TAG, "Starting server...");
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock_fd < 0)
    {
        ESP_LOGE(TAG, "Faled to create socket. Err: %i", errno);
        vTaskDelete(NULL);
    }

    struct sockaddr_in listen_addr = {
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = lwip_htons(11111),
        .sin_family = AF_INET
    };
    if (bind(sock_fd, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) == 0)
    {
        ESP_LOGI(TAG, "Bound to port: %i", lwip_ntohs(listen_addr.sin_port));
    }
    else
    {
        ESP_LOGE(TAG, "Faled to bind to socket. Err: %i", errno);
        vTaskDelete(NULL);
    }

    const size_t buf_size = 2048;
    uint8_t* buf = malloc(buf_size);
    led_color* leds = malloc(sizeof(led_color) * 600);
    for (;;)
    {
        // ESP_LOGI(TAG, "Wait for msg");
        const size_t rcv_len = recv(sock_fd, buf, buf_size, 0);
        if (rcv_len == 4 && strncmp("kill", (char*)buf, 4) == 0)
        {
            break;
        }

        // validate msg format (expect 3 * N bytes)
        const int len = rcv_len / 3;
        if (len < 1 || rcv_len % 3 != 0)
        {
            ESP_LOGW(TAG, "Invalid message format");
            continue;
        }

        const uint8_t* colors = buf;
        for(int i = 0; i < len; i++)
        {
            const int offset = i * 3;
            const led_color color = {
                .red = colors[offset + 0],
                .green = colors[offset + 1],
                .blue = colors[offset + 2]
            };
            leds[i] = color;
        }
        submit_update(len, leds);
    }
    free(buf);
    if (closesocket(sock_fd) != 0)
    {
        ESP_LOGE(TAG, "Failed to close socket. Errno: %i", errno);
    }
    ESP_LOGI(TAG, "Terminating server task");
    vTaskDelete(NULL);
}
