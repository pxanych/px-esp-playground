/*
 * Created by pxanych on 15.10.2024.
 *
 * Based on Bosh BMP280 Datasheet: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf
 */
#include <bmp280.h>
#include <driver/i2c_master.h>
#include <esp_log.h>

void bmp280_calculate_measurements(int32_t adc_t, int32_t adc_p, bmp280_device* device_handle);

esp_err_t bmp280_init(i2c_master_bus_handle_t i2c_bus_handle, uint16_t device_address, bmp280_device *bmp280_handle) {
    ESP_LOGI("bmp280_init", "Setting up i2c device");
    i2c_device_config_t deviceConfig = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = device_address,
            .scl_speed_hz = 100000, // 100kHz
    };

    i2c_master_dev_handle_t i2c_device_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &deviceConfig, &i2c_device_handle));
    bmp280_handle->device_handle = i2c_device_handle;

    uint8_t send_buf[2] = {BMP280_REG_RESET, BMP280_RESET};
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, send_buf, 2, 1000));

    send_buf[0] = BMP280_REG_ID;
    uint8_t chip_id;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_handle, send_buf, 1,
                                                &chip_id, 1, 1000));
    ESP_ERROR_CHECK(chip_id != BMP280_CHIP_ID);


    send_buf[0] = BMP280_REG_CALIB_START;
    uint8_t rcv_size = BMP280_REG_CALIB_END - BMP280_REG_CALIB_START + 1;
    uint8_t *rcv_buf = (uint8_t *) (&(bmp280_handle->calibration_data));
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_handle, send_buf, 1,
                                                rcv_buf, rcv_size, 1000));

    return ESP_OK;
}

esp_err_t bmp280_set_config(bmp280_device *bmp280_handle, bmp280_ctrl_meas measure_settings, bmp280_config_t config) {
    uint8_t send_buf[4] = {BMP280_REG_CTRL_MEAS, measure_settings.byte, BMP280_REG_CONFIG, config.byte};
    return i2c_master_transmit(bmp280_handle->device_handle, send_buf, 4, 1000);
}

esp_err_t bmp280_read_data(bmp280_device *bmp280_handle) {


    uint8_t send_buf = BMP280_REG_PRESS_MSB;
    uint8_t recv_buf[6];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(bmp280_handle->device_handle, &send_buf, 1,
                                                recv_buf, 6, 1000));
    uint32_t adc_p = (recv_buf[0] << 12) | (recv_buf[1] << 4) | (recv_buf[2] >> 4);
    uint32_t adc_t = (recv_buf[3] << 12) | (recv_buf[4] << 4) | (recv_buf[5] >> 4);
    bmp280_calculate_measurements((int32_t)adc_t, (int32_t)adc_p, bmp280_handle);
    return ESP_OK;
}

void bmp280_calculate_measurements(int32_t adc_t, int32_t adc_p, bmp280_device* device_handle) {
    bmp280_calib_data *calib_data = &(device_handle->calibration_data);
    int32_t dig_T1 = (int32_t) calib_data->dig_T1;
    int32_t dig_T2 = (int32_t) calib_data->dig_T2;
    int32_t dig_T3 = (int32_t) calib_data->dig_T3;
    int64_t dig_P1 = (int64_t) calib_data->dig_P1;
    int64_t dig_P2 = (int64_t) calib_data->dig_P2;
    int64_t dig_P3 = (int64_t) calib_data->dig_P3;
    int64_t dig_P4 = (int64_t) calib_data->dig_P4;
    int64_t dig_P5 = (int64_t) calib_data->dig_P5;
    int64_t dig_P6 = (int64_t) calib_data->dig_P6;
    int64_t dig_P7 = (int64_t) calib_data->dig_P7;
    int64_t dig_P8 = (int64_t) calib_data->dig_P8;
    int64_t dig_P9 = (int64_t) calib_data->dig_P9;

    int32_t var1, var2, t_fine, t;
    int64_t var3, var4, p;

    // temperature compensation
    var1 = ((((adc_t >> 3) - (dig_T1 << 1))) * (dig_T2)) >> 11;
    var2 = (((((adc_t >> 4) - dig_T1) * ((adc_t >> 4) - dig_T1)) >> 12) * dig_T3) >> 14;
    t_fine = var1 + var2;
    t = (t_fine * 5 + 128) >> 8;

    // pressure compensation
    var3 = ((int64_t ) t_fine) - 128000;
    var4 = var3 * var3 * dig_P6;
    var4 = var4 + ((var3 * dig_P5) << 17);
    var4 = var4 + (dig_P4 << 35);
    var3 = ((var3 * var3 * dig_P3) >> 8) + ((var3 * dig_P2) << 12);
    var3 = (((((int64_t) 1) << 47) + var3)) * dig_P1 >> 33;
    if (var3 == 0) {
        p = 0; // avoid exception caused by division by zero
    } else {
        p = 1048576 - adc_p;
        p = (((p << 31) - var4) * 3125) / var3;
        var3 = (dig_P9 * (p >> 13) * (p >> 13)) >> 25;
        var4 = (dig_P8 * p) >> 19;
        p = ((p + var3 + var4) >> 8) + (dig_P7 << 4);
    }

    device_handle->measure_data.deg_c_x100 = t;
    device_handle->measure_data.pa_x256 = p;
}