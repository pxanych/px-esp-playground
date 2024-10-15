#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <bmp280.h>
#include <driver/i2c_master.h>
#include <esp_log.h>

void app_main(void) {
    i2c_master_bus_handle_t masterBusHandle;
    i2c_master_bus_config_t masterBusConfig = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1, // autoselect
            .scl_io_num = GPIO_NUM_4,
            .sda_io_num = GPIO_NUM_5,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&masterBusConfig, &masterBusHandle));

    bmp280_device device;
    bmp280_init(masterBusHandle, BMP280_ADDR_0, &device);

    bmp280_ctrl_meas ctrl_meas = {
            .bits.osrs_t = 0b010,   // x2  oversampling
            .bits.osrs_p = 0b101,   // x16 oversampling
            .bits.mode = 0b11,      // normal mode
    };
    bmp280_config_t bmp_config = {
            .bits.t_sb = 0b100,      // measure period 500ms
            .bits.filter = 0b101,    // filter x16
            .bits.spi3w_en = 0,
    };

    bmp280_set_config(&device, ctrl_meas, bmp_config);

    esp_rom_delay_us(1000000);

    while (1) {
        esp_rom_delay_us(1000000);
        bmp280_read_data(&device);
        uint32_t p = device.measure_data.pa_x256 * 100 / 256;
        uint32_t t = device.measure_data.deg_c_x100;
        ESP_LOGI("app_main", "%li.%02li degC, %li.%02li Pa", t / 100, t % 100, p / 100, p % 100);
    }

//    vTaskDelete(NULL);
}
