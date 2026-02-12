#pragma once

#include "i2c_bsp.hpp"
#include <cstdint>

namespace sensors {

/// SHTC3 temperature/humidity sensor driver (I2C address 0x70)
class Shtc3 {
public:
    explicit Shtc3(I2cMasterBus& bus);
    ~Shtc3();

    /// Initialize sensor, verify ID
    bool init();

    /// Read temperature and humidity
    /// @param[out] tempC Temperature in Celsius
    /// @param[out] humidity Relative humidity percentage (0-100)
    /// @return true if read succeeded with valid CRC
    bool read(float* tempC, float* humidity);

    /// Enter sleep mode (low power)
    void sleep();

    /// Wake from sleep mode
    void wakeup();

private:
    I2cMasterBus& bus_;
    i2c_master_dev_handle_t dev_;
    uint16_t id_;

    static constexpr uint8_t ADDR = 0x70;
    static constexpr uint32_t SPEED_HZ = 400000;  // 400 kHz

    // Commands (big-endian, sent MSB first)
    static constexpr uint16_t CMD_READ_ID = 0xEFC8;
    static constexpr uint16_t CMD_SOFT_RESET = 0x805D;
    static constexpr uint16_t CMD_SLEEP = 0xB098;
    static constexpr uint16_t CMD_WAKEUP = 0x3517;
    static constexpr uint16_t CMD_MEASURE = 0x7866;  // T first, normal mode, no clock stretch

    /// Send a 16-bit command
    esp_err_t sendCommand(uint16_t cmd);

    /// Check CRC-8 over data bytes
    static bool checkCrc(const uint8_t* data, size_t len, uint8_t checksum);

    /// Convert raw sensor value to temperature
    static float calcTemperature(uint16_t raw);

    /// Convert raw sensor value to humidity
    static float calcHumidity(uint16_t raw);
};

}  // namespace sensors
