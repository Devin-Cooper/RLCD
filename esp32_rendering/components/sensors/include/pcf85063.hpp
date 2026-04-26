#pragma once

#include "i2c_bsp.hpp"
#include <cstdint>

namespace sensors {

struct RtcTime {
    uint16_t year;      // 2000-2099
    uint8_t month;      // 1-12
    uint8_t day;        // 1-31
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t second;     // 0-59
    uint8_t weekday;    // 0=Sun, 6=Sat
};

/// PCF85063 RTC driver (I2C address 0x51)
class Pcf85063 {
public:
    explicit Pcf85063(i2c_bsp::I2cMasterBus& bus);
    ~Pcf85063();

    /// Initialize the RTC (resets OS flag, starts oscillator)
    bool init();

    /// True if the OS (oscillator stop) flag was set when init() ran.
    /// Latched at init time so callers can detect "RTC unset at boot".
    bool oscillatorStoppedAtBoot() const;

    /// Read current time (legacy, no error reporting)
    RtcTime getTime();

    /// Read current time with error/bounds checking.
    /// Returns false on I2C error or out-of-range BCD values.
    bool getTime(RtcTime& out);

    /// Set time
    void setTime(const RtcTime& time);

private:
    i2c_bsp::I2cMasterBus& bus_;
    i2c_master_dev_handle_t dev_;
    bool osStoppedAtBoot_ = false;

    static constexpr uint8_t ADDR = 0x51;
    static constexpr uint32_t SPEED_HZ = 300000;  // 300 kHz

    // Register addresses
    static constexpr uint8_t REG_CONTROL1 = 0x00;
    static constexpr uint8_t REG_CONTROL2 = 0x01;
    static constexpr uint8_t REG_SECONDS = 0x04;
    static constexpr uint8_t REG_MINUTES = 0x05;
    static constexpr uint8_t REG_HOURS = 0x06;
    static constexpr uint8_t REG_DAYS = 0x07;
    static constexpr uint8_t REG_WEEKDAYS = 0x08;
    static constexpr uint8_t REG_MONTHS = 0x09;
    static constexpr uint8_t REG_YEARS = 0x0A;

    static uint8_t bcdToDec(uint8_t bcd);
    static uint8_t decToBcd(uint8_t dec);
};

}  // namespace sensors
