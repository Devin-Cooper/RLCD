#pragma once

#include <driver/i2c.h>

class I2cMasterBus {
private:
    i2c_port_t i2c_port_;

public:
    I2cMasterBus(int scl_pin, int sda_pin, int i2c_port = 0);
    ~I2cMasterBus();

    int i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data);
    int i2c_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data);
    int i2c_write_bytes(uint8_t dev_addr, uint8_t *data, size_t len);
    i2c_port_t get_port() { return i2c_port_; }
};
