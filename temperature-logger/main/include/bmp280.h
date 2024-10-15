/*
 * Created by pxanych on 15.10.2024.
 *
 * Based on Bosh BMP280 Datasheet: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf
 */

#ifndef TEMPERATURE_LOGGER_BMP280_H
#define TEMPERATURE_LOGGER_BMP280_H

#include <stdint.h>
#include <esp_err.h>
#include <driver/i2c_types.h>

// Memory map
#define BMP280_REG_TEMP_XLSB 0xFC
#define BMP280_REG_TEMP_LSB 0xFB
#define BMP280_REG_TEMP_MSB 0xFA
#define BMP280_REG_PRESS_XLSB 0xF9
#define BMP280_REG_PRESS_LSB 0xF8
#define BMP280_REG_PRESS_MSB 0xF7

#define BMP280_REG_CONFIG 0xF5
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_STATUS 0xF3
#define BMP280_REG_RESET 0xE0
#define BMP280_REG_ID 0xD0
#define BMP280_REG_CALIB_START 0x88
#define BMP280_REG_CALIB_END 0xA1


#define BMP280_CHIP_ID 0x58
#define BMP280_RESET 0xB6

#define BMP280_ADDR_0 0x76
#define BMP280_ADDR_1 0x77

typedef union bmp280_config_t {
    struct __attribute__((packed)) {
        uint8_t spi3w_en: 1;
        uint8_t : 1;
        uint8_t filter: 3;
        uint8_t t_sb: 3;
    } bits;
    uint8_t byte;
} bmp280_config_t;

typedef union bmp280_ctrl_meas {
    struct __attribute__((packed)) {
        uint8_t mode: 2;
        uint8_t osrs_p: 3;
        uint8_t osrs_t: 3;
    } bits;
    uint8_t byte;
} bmp280_ctrl_meas;

typedef union bmp280_status {
    struct __attribute__((packed)) {
        uint8_t im_update: 1;
        uint8_t : 2;
        uint8_t measuring: 1;
        uint8_t : 4;
    } bits;
    uint8_t byte;
} bmp280_status;

typedef struct __attribute__((packed)) bmp280_calib_data {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    int16_t reserved;
} bmp280_calib_data;

typedef struct bmp280_measure_data {
    int32_t deg_c_x100;
    uint32_t pa_x256;
} bmp280_measure_data;

typedef struct bmp280_device {
    i2c_master_dev_handle_t device_handle;
    bmp280_calib_data calibration_data;
    bmp280_measure_data measure_data;
} bmp280_device;

esp_err_t bmp280_init(i2c_master_bus_handle_t i2c_bus_handle, uint16_t device_address, bmp280_device *bmp280_handle);

esp_err_t bmp280_set_config(bmp280_device *bmp280_handle, bmp280_ctrl_meas measure_settings, bmp280_config_t config);

esp_err_t bmp280_read_data(bmp280_device *bmp280_handle);

#endif //TEMPERATURE_LOGGER_BMP280_H
