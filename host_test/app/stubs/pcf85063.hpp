#pragma once
// Host-test stub for sensors::Pcf85063. Real version touches I2C hardware.
// Field order/types of RtcTime must match esp32_rendering/components/sensors/include/pcf85063.hpp.
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

class Pcf85063 {
public:
    explicit Pcf85063(i2c_bsp::I2cMasterBus&) {}
    bool init() { return true; }
    bool oscillatorStoppedAtBoot() const { return false; }
    RtcTime getTime() { return {}; }
    bool getTime(RtcTime& out) { out = {}; return true; }
    void setTime(const RtcTime&) {}
};

}  // namespace sensors
