#include "i2c_bsp.hpp"
#include <esp_log.h>

static const char* TAG = "i2c_bsp";

I2cMasterBus::I2cMasterBus(gpio_num_t scl, gpio_num_t sda, i2c_port_t port)
    : handle_(nullptr) {
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = port;
    bus_config.scl_io_num = scl;
    bus_config.sda_io_num = sda;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(err));
        handle_ = nullptr;
    } else {
        ESP_LOGI(TAG, "I2C master bus initialized (SCL=%d, SDA=%d)", scl, sda);
    }
}

I2cMasterBus::~I2cMasterBus() {
    if (handle_) {
        i2c_del_master_bus(handle_);
    }
}

esp_err_t I2cMasterBus::addDevice(uint8_t addr, uint32_t speedHz,
                                   i2c_master_dev_handle_t* devHandle) {
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = addr;
    dev_config.scl_speed_hz = speedHz;

    esp_err_t err = i2c_master_bus_add_device(handle_, &dev_config, devHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device 0x%02X: %s", addr, esp_err_to_name(err));
    }
    return err;
}

esp_err_t I2cMasterBus::writeReg(i2c_master_dev_handle_t dev, uint8_t reg,
                                  const uint8_t* data, size_t len) {
    // Combine register address and data into one write
    // Max I2C write: 1 reg byte + up to 15 data bytes (RTC writes 7 max)
    constexpr size_t MAX_BUF = 16;
    if (len + 1 > MAX_BUF) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t buf[MAX_BUF];
    buf[0] = reg;
    if (data && len > 0) {
        memcpy(buf + 1, data, len);
    }
    return i2c_master_transmit(dev, buf, len + 1, -1);
}

esp_err_t I2cMasterBus::readReg(i2c_master_dev_handle_t dev, uint8_t reg,
                                 uint8_t* data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

esp_err_t I2cMasterBus::writeRead(i2c_master_dev_handle_t dev,
                                   const uint8_t* wbuf, size_t wlen,
                                   uint8_t* rbuf, size_t rlen) {
    return i2c_master_transmit_receive(dev, wbuf, wlen, rbuf, rlen, -1);
}
