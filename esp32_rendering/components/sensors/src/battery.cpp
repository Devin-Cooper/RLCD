#include "battery.hpp"
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

static const char* TAG = "battery";

// ADC handle (module-level for simplicity)
static adc_oneshot_unit_handle_t adc1_handle = nullptr;
static adc_cali_handle_t cali_handle = nullptr;

namespace sensors {

Battery::Battery()
    : initialized_(false)
    , smoothedMv_(3600 * SMOOTH_SAMPLES) {  // Start at ~50%
}

bool Battery::init() {
    if (initialized_) return true;

    // Configure ADC1
    adc_oneshot_unit_init_cfg_t init_cfg = {};
    init_cfg.unit_id = ADC_UNIT_1;
    init_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &adc1_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC1: %s", esp_err_to_name(err));
        return false;
    }

    // Configure channel (GPIO1 = ADC1_CH0)
    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = ADC_ATTEN_DB_12;  // 0-3.3V range (actually ~0-3.1V usable)
    chan_cfg.bitwidth = ADC_BITWIDTH_12;

    err = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(err));
        return false;
    }

    // Set up calibration for accurate voltage reading
    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id = ADC_UNIT_1;
    cali_cfg.chan = ADC_CHANNEL_0;
    cali_cfg.atten = ADC_ATTEN_DB_12;
    cali_cfg.bitwidth = ADC_BITWIDTH_12;

    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
        cali_handle = nullptr;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Battery monitor initialized on GPIO%d", BATTERY_GPIO);
    return true;
}

uint16_t Battery::readMillivolts() {
    if (!initialized_ || !adc1_handle) return 0;

    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return 0;
    }

    int voltage_mv = 0;
    if (cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv);
    } else {
        // Fallback: approximate conversion (3100mV max at 12dB atten)
        voltage_mv = (raw * 3100) / 4095;
    }

    // Apply voltage divider ratio to get actual battery voltage
    uint16_t battery_mv = static_cast<uint16_t>(voltage_mv * DIVIDER_RATIO);

    return battery_mv;
}

uint8_t Battery::readPercent() {
    uint16_t mv = readMillivolts();

    if (mv <= VBAT_MIN_MV) return 0;
    if (mv >= VBAT_MAX_MV) return 100;

    // Linear interpolation
    uint32_t percent = (static_cast<uint32_t>(mv - VBAT_MIN_MV) * 100) /
                       (VBAT_MAX_MV - VBAT_MIN_MV);
    return static_cast<uint8_t>(percent);
}

uint8_t Battery::readPercentSmoothed() {
    uint16_t mv = readMillivolts();

    // Exponential moving average
    smoothedMv_ = smoothedMv_ - (smoothedMv_ / SMOOTH_SAMPLES) + mv;
    uint16_t avgMv = smoothedMv_ / SMOOTH_SAMPLES;

    if (avgMv <= VBAT_MIN_MV) return 0;
    if (avgMv >= VBAT_MAX_MV) return 100;

    uint32_t percent = (static_cast<uint32_t>(avgMv - VBAT_MIN_MV) * 100) /
                       (VBAT_MAX_MV - VBAT_MIN_MV);
    return static_cast<uint8_t>(percent);
}

}  // namespace sensors
