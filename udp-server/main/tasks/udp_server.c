//
// Created by pxanych on 18.12.2023.
//

#include "freertos/FreeRTOS.h"
#include "tasks/udp_server.h"

#include <esp_log.h>
#include <lwip/sockets.h>

static const char* SERVER_TASK_TAG = "server_task";

void server_task(void* pvArgs)
{
    ESP_LOGI(SERVER_TASK_TAG, "Starting server...");
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock_fd < 0)
    {
        ESP_LOGE(SERVER_TASK_TAG, "Faled to create socket. Err: %i", errno);
        vTaskDelete(NULL);
    }

    struct sockaddr_in listen_addr = {
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = lwip_htons(11111),
        .sin_family = AF_INET
    };
    if (bind(sock_fd, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) == 0)
    {
        ESP_LOGI(SERVER_TASK_TAG, "Bound to port: %i", lwip_ntohs(listen_addr.sin_port));
    }
    else
    {
        ESP_LOGE(SERVER_TASK_TAG, "Faled to bind to socket. Err: %i", errno);
        vTaskDelete(NULL);
    }

    size_t buf_size = 2048;
    char* buf = malloc(buf_size);
    for (;;)
    {
        memset(buf, 0, buf_size);
        ESP_LOGI(SERVER_TASK_TAG, "Wait for msg");
        size_t rcv_len = recv(sock_fd, buf, buf_size, 0);
        if (rcv_len > 100)
        {
            char msg[100];
            memset(msg, 0, 100);
            memcpy(msg, buf, 99);
            ESP_LOGI(SERVER_TASK_TAG, "New msg (%i bytes): %s...", rcv_len, msg);
        } else
        {
            ESP_LOGI(SERVER_TASK_TAG, "New msg (%i bytes): %s", rcv_len, buf);
        }

        if (rcv_len == 4 && strcmp("kill", buf) == 0)
        {
            break;
        }
    }
    free(buf);
    if (closesocket(sock_fd) != 0)
    {
        ESP_LOGE(SERVER_TASK_TAG, "Failed to close socket. Errno: %i", errno);
    }
    ESP_LOGI(SERVER_TASK_TAG, "Terminating server task");
    vTaskDelete(NULL);
}
