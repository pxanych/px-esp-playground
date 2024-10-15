//
// Created by pxany on 06.10.2024.
//

#include <sensor_init.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

static i2c_master_bus_handle_t masterBusHandle;
static i2c_master_dev_handle_t deviceHandle;

void sensor_init_task(void* pvArgs) {
    char *task_name = pcTaskGetName(NULL);

    ESP_LOGI(task_name, "Setting up i2c bus");
    i2c_master_bus_config_t masterBusConfig = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1, // autoselect
            .scl_io_num = GPIO_NUM_4,
            .sda_io_num = GPIO_NUM_5,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&masterBusConfig, &masterBusHandle));

    ESP_LOGI(task_name, "Setting up i2c device");
    i2c_device_config_t deviceConfig = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x76,
            .scl_speed_hz = 100000, // 100kHz
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(masterBusHandle, &deviceConfig, &deviceHandle));

    uint8_t *sendBuf = malloc(128);
    uint8_t *receiveBuf = malloc(128);

    ESP_LOGI(task_name, "Reading chip id");
    sendBuf[0] = 0xD0;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(deviceHandle, sendBuf, 1, receiveBuf, 1, 1000));
    ESP_LOGI(task_name, "Chip id: %hhX", receiveBuf[0]);


    ESP_LOGI(task_name, "Scheduling a task to delete i2c bus");
    xTaskCreate(
            sensor_shutdown_task,
            "sensor_shutdown_task",
            2048,
            NULL,
            1,
            NULL
    );

    ESP_LOGI(task_name, "Will exit, core: %d", xPortGetCoreID());
    vTaskDelete(0); // delete self
}



void sensor_shutdown_task(void* pvArgs) {
    char *task_name = pcTaskGetName(NULL);



    ESP_LOGI(task_name, "Will remove i2c device");
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(deviceHandle));

    ESP_LOGI(task_name, "Will remove i2c bus");
    ESP_ERROR_CHECK(i2c_del_master_bus(masterBusHandle));

    ESP_LOGI(task_name, "Will exit, core: %d", xPortGetCoreID());
    vTaskDelete(NULL); // delete self
}