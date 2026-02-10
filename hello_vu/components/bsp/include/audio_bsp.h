#pragma once

#include "i2c_bsp.h"

class AudioPort {
private:
    I2cMasterBus &i2cbus_;

    static constexpr uint8_t ES7210_ADDR = 0x40;

    void es7210_write_reg(uint8_t reg, uint8_t val);

public:
    AudioPort(I2cMasterBus &i2cbus);
    ~AudioPort();

    bool init();
    void setMicGain(float db);
    int readMicData(void *buffer, int len);
};
