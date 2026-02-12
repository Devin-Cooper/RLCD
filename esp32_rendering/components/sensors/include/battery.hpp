#pragma once

#include <cstdint>

namespace sensors {

/// Battery voltage monitor using ADC
/// Waveshare ESP32-S3-RLCD-4.2 uses GPIO1 with 3:1 voltage divider
class Battery {
public:
    Battery();

    /// Initialize ADC for battery reading
    bool init();

    /// Read battery voltage in millivolts (3000-4200 typical for 18650)
    uint16_t readMillivolts();

    /// Read battery percentage (0-100)
    /// Uses simple linear mapping: 3.0V = 0%, 4.2V = 100%
    uint8_t readPercent();

    /// Read smoothed percentage (averages multiple samples)
    uint8_t readPercentSmoothed();

private:
    bool initialized_;

    // Smoothing state
    uint32_t smoothedMv_;

    static constexpr int BATTERY_GPIO = 1;           // GPIO1 = ADC1_CH0
    static constexpr float DIVIDER_RATIO = 3.0f;     // 200K + 100K divider
    static constexpr uint16_t VBAT_MIN_MV = 3000;    // Empty
    static constexpr uint16_t VBAT_MAX_MV = 4200;    // Full
    static constexpr int SMOOTH_SAMPLES = 8;
};

}  // namespace sensors
