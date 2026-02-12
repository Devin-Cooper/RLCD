#include "pcf85063.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "pcf85063";

namespace sensors {

Pcf85063::Pcf85063(I2cMasterBus& bus)
    : bus_(bus)
    , dev_(nullptr) {
}

Pcf85063::~Pcf85063() {
    if (dev_) {
        i2c_master_bus_rm_device(dev_);
    }
}

bool Pcf85063::init() {
    esp_err_t err = bus_.addDevice(ADDR, SPEED_HZ, &dev_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add PCF85063 device");
        return false;
    }

    // Read Control_1 register to check oscillator stop flag
    uint8_t ctrl1 = 0;
    err = bus_.readReg(dev_, REG_CONTROL1, &ctrl1, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read Control_1 register");
        return false;
    }

    // Bit 5 = OS (oscillator stop). If set, clock integrity is not guaranteed.
    if (ctrl1 & 0x20) {
        ESP_LOGW(TAG, "Oscillator was stopped, clearing OS flag");
        ctrl1 &= ~0x20;
        bus_.writeReg(dev_, REG_CONTROL1, &ctrl1, 1);
    }

    ESP_LOGI(TAG, "PCF85063 initialized (Control_1=0x%02X)", ctrl1);
    return true;
}

RtcTime Pcf85063::getTime() {
    RtcTime time = {};

    // Read 7 time registers starting at REG_SECONDS (0x04)
    uint8_t regs[7];
    esp_err_t err = bus_.readReg(dev_, REG_SECONDS, regs, 7);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time registers");
        return time;
    }

    // Parse BCD values (masking out unused bits)
    time.second = bcdToDec(regs[0] & 0x7F);  // Bit 7 = OS flag
    time.minute = bcdToDec(regs[1] & 0x7F);
    time.hour = bcdToDec(regs[2] & 0x3F);    // 24-hour mode
    time.day = bcdToDec(regs[3] & 0x3F);
    time.weekday = regs[4] & 0x07;
    time.month = bcdToDec(regs[5] & 0x1F);
    time.year = 2000 + bcdToDec(regs[6]);

    return time;
}

void Pcf85063::setTime(const RtcTime& time) {
    uint8_t regs[7];
    regs[0] = decToBcd(time.second);
    regs[1] = decToBcd(time.minute);
    regs[2] = decToBcd(time.hour);
    regs[3] = decToBcd(time.day);
    regs[4] = time.weekday;
    regs[5] = decToBcd(time.month);
    regs[6] = decToBcd(static_cast<uint8_t>(time.year - 2000));

    esp_err_t err = bus_.writeReg(dev_, REG_SECONDS, regs, 7);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set time");
    }
}

uint8_t Pcf85063::bcdToDec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

uint8_t Pcf85063::decToBcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

}  // namespace sensors
