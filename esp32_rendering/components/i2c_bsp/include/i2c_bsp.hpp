#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <cstdint>
#include <cstddef>

/// I2C master bus wrapper for ESP-IDF new I2C driver
class I2cMasterBus {
public:
    /// Initialize I2C master bus
    /// @param scl SCL GPIO pin
    /// @param sda SDA GPIO pin
    /// @param port I2C port number
    I2cMasterBus(gpio_num_t scl, gpio_num_t sda, i2c_port_t port = I2C_NUM_0);
    ~I2cMasterBus();

    // Non-copyable
    I2cMasterBus(const I2cMasterBus&) = delete;
    I2cMasterBus& operator=(const I2cMasterBus&) = delete;

    /// Get underlying bus handle for adding devices
    i2c_master_bus_handle_t handle() const { return handle_; }

    /// Add a device to this bus
    /// @param addr 7-bit I2C address
    /// @param speedHz SCL frequency for this device
    /// @param[out] devHandle Output device handle
    esp_err_t addDevice(uint8_t addr, uint32_t speedHz,
                        i2c_master_dev_handle_t* devHandle);

    /// Write to a register
    esp_err_t writeReg(i2c_master_dev_handle_t dev, uint8_t reg,
                       const uint8_t* data, size_t len);

    /// Read from a register
    esp_err_t readReg(i2c_master_dev_handle_t dev, uint8_t reg,
                      uint8_t* data, size_t len);

    /// Write then read (combined transaction)
    esp_err_t writeRead(i2c_master_dev_handle_t dev,
                        const uint8_t* wbuf, size_t wlen,
                        uint8_t* rbuf, size_t rlen);

private:
    i2c_master_bus_handle_t handle_;
};
