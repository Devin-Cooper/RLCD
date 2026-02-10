#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "i2c_bsp.h"

static const char *TAG = "I2CBSP";

I2cMasterBus::I2cMasterBus(int scl_pin, int sda_pin, int i2c_port) {
    i2c_port_ = (i2c_port_t)i2c_port;

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)sda_pin;
    conf.scl_io_num = (gpio_num_t)scl_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    ESP_ERROR_CHECK(i2c_param_config(i2c_port_, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_port_, conf.mode, 0, 0, 0));

    ESP_LOGI(TAG, "I2C master initialized on port %d (SDA=%d, SCL=%d)", i2c_port, sda_pin, scl_pin);
}

I2cMasterBus::~I2cMasterBus() {
    i2c_driver_delete(i2c_port_);
}

int I2cMasterBus::i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port_, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

int I2cMasterBus::i2c_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port_, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

int I2cMasterBus::i2c_write_bytes(uint8_t dev_addr, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port_, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}
