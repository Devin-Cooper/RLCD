#include "shtc3.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

static const char* TAG = "shtc3";

namespace sensors {

Shtc3::Shtc3(I2cMasterBus& bus)
    : bus_(bus)
    , dev_(nullptr)
    , id_(0) {
}

Shtc3::~Shtc3() {
    if (dev_) {
        i2c_master_bus_rm_device(dev_);
    }
}

bool Shtc3::init() {
    esp_err_t err = bus_.addDevice(ADDR, SPEED_HZ, &dev_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SHTC3 device");
        return false;
    }

    // Wake up first (in case it's sleeping)
    wakeup();
    vTaskDelay(pdMS_TO_TICKS(1));

    // Soft reset
    sendCommand(CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read ID register
    uint8_t cmd[2] = {
        static_cast<uint8_t>(CMD_READ_ID >> 8),
        static_cast<uint8_t>(CMD_READ_ID & 0xFF)
    };
    uint8_t rbuf[3];
    err = bus_.writeRead(dev_, cmd, 2, rbuf, 3);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ID");
        return false;
    }

    id_ = (static_cast<uint16_t>(rbuf[0]) << 8) | rbuf[1];

    // Verify SHTC3 ID: bits [5:0] and [11] should match 0x0807 mask
    if ((id_ & 0x083F) != 0x0807) {
        ESP_LOGE(TAG, "Unexpected sensor ID: 0x%04X", id_);
        return false;
    }

    ESP_LOGI(TAG, "SHTC3 initialized (ID=0x%04X)", id_);
    return true;
}

bool Shtc3::read(float* tempC, float* humidity) {
    // Wake sensor
    wakeup();
    vTaskDelay(pdMS_TO_TICKS(1));

    // Start measurement (T first, normal mode, no clock stretching)
    sendCommand(CMD_MEASURE);
    vTaskDelay(pdMS_TO_TICKS(15));  // Max measurement time ~12.1ms

    // Read 6 bytes: T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC
    uint8_t data[6];
    esp_err_t err = i2c_master_receive(dev_, data, 6, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read measurement data");
        return false;
    }

    // Verify CRCs
    if (!checkCrc(data, 2, data[2])) {
        ESP_LOGE(TAG, "Temperature CRC mismatch");
        return false;
    }
    if (!checkCrc(data + 3, 2, data[5])) {
        ESP_LOGE(TAG, "Humidity CRC mismatch");
        return false;
    }

    // Parse raw values
    uint16_t rawTemp = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    uint16_t rawHumid = (static_cast<uint16_t>(data[3]) << 8) | data[4];

    *tempC = calcTemperature(rawTemp);
    *humidity = calcHumidity(rawHumid);

    // Put sensor back to sleep
    sleep();

    return true;
}

void Shtc3::sleep() {
    sendCommand(CMD_SLEEP);
}

void Shtc3::wakeup() {
    sendCommand(CMD_WAKEUP);
}

esp_err_t Shtc3::sendCommand(uint16_t cmd) {
    uint8_t buf[2] = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF)
    };
    return i2c_master_transmit(dev_, buf, 2, -1);
}

bool Shtc3::checkCrc(const uint8_t* data, size_t len, uint8_t checksum) {
    // CRC-8 with polynomial 0x31, init 0xFF
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc == checksum;
}

float Shtc3::calcTemperature(uint16_t raw) {
    // Formula from datasheet: T = -45 + 175 * (raw / 65535)
    return -45.0f + 175.0f * (static_cast<float>(raw) / 65535.0f);
}

float Shtc3::calcHumidity(uint16_t raw) {
    // Formula from datasheet: RH = 100 * (raw / 65535)
    return 100.0f * (static_cast<float>(raw) / 65535.0f);
}

}  // namespace sensors
