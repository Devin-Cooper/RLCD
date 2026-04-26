#include "microphone.hpp"
#include "audio_helpers.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>
#include <cstring>

static const char* TAG = "microphone";
static constexpr const char* kNvsNs = "audio";
static constexpr const char* kNvsGainL = "mic_gain_l";
static constexpr const char* kNvsGainR = "mic_gain_r";
static constexpr const char* kNvsAlc   = "mic_alc";

namespace audio {

Microphone::Microphone(audio_bus::AudioBus& bus, i2c_bsp::I2cMasterBus& i2c, uint8_t addr)
    : bus_(bus), i2c_(i2c), addr_(addr) {
    mtx_ = xSemaphoreCreateMutex();
}
Microphone::~Microphone() {
    if (codec_dev_) i2c_master_bus_rm_device(codec_dev_);
    if (mtx_) vSemaphoreDelete(mtx_);
}

void Microphone::writeReg(uint8_t reg, uint8_t v) {
    if (!codec_dev_) return;
    i2c_.writeReg(codec_dev_, reg, &v, 1);
}
uint8_t Microphone::readReg(uint8_t reg) {
    if (!codec_dev_) return 0;
    uint8_t v = 0;
    i2c_.readReg(codec_dev_, reg, &v, 1);
    return v;
}

void Microphone::load() {
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) return;
    int8_t v;
    if (nvs_get_i8(h, kNvsGainL, &v) == ESP_OK) gain_l_ = v;
    if (nvs_get_i8(h, kNvsGainR, &v) == ESP_OK) gain_r_ = v;
    uint8_t a = 0;
    if (nvs_get_u8(h, kNvsAlc, &a) == ESP_OK) alc_ = (a != 0);
    nvs_close(h);
}
void Microphone::persist() {
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i8(h, kNvsGainL, gain_l_);
    nvs_set_i8(h, kNvsGainR, gain_r_);
    nvs_set_u8(h, kNvsAlc, alc_ ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

bool Microphone::init() {
    load();
    if (i2c_.addDevice(addr_, 100000, &codec_dev_) != ESP_OK) {
        ESP_LOGE(TAG, "ES7210 addDevice 0x%02X failed", addr_);
        return false;
    }
    // Probe by reading reg 0x00.
    {
        uint8_t r0 = 0;
        if (i2c_.readReg(codec_dev_, 0x00, &r0, 1) != ESP_OK) {
            ESP_LOGE(TAG, "ES7210 not responding at 0x%02X", addr_);
            i2c_master_bus_rm_device(codec_dev_);
            codec_dev_ = nullptr;
            return false;
        }
    }
    ESP_LOGI(TAG, "ES7210 found at 0x%02X", addr_);

    // ---- Soft reset ----
    writeReg(0x00, 0xFF);
    writeReg(0x00, 0x32);
    // ---- Init time / power-up time ----
    writeReg(0x09, 0x30);
    writeReg(0x0A, 0x30);
    // ---- HPF for ADC1-4 ----
    writeReg(0x23, 0x2A);
    writeReg(0x22, 0x0A);
    writeReg(0x21, 0x2A);
    writeReg(0x20, 0x0A);
    // ---- I²S format: I2S, 16-bit, no TDM ----
    //   reg 0x11 = bit_width_field | i2s_format = 0x60 (16b) | 0x00 (I2S) = 0x60
    //   reg 0x12 = 0x00 (TDM disabled)
    writeReg(0x11, 0x60);
    writeReg(0x12, 0x00);
    // ---- Analog/VMID power ----
    writeReg(0x40, 0xC3);
    // ---- Mic bias 2.87 V on MIC12 + MIC34 ----
    //   driver enum ES7210_MIC_BIAS_2V87 = 0x70 (verified in include/es7210.h).
    //   The bias field is the upper 4 bits, NOT the lower 3 — earlier draft of
    //   this plan said 0x07 (off-by-shift); corrected during review 2026-04-26.
    writeReg(0x41, 0x70);
    writeReg(0x42, 0x70);
    // ---- Per-channel PGA gain (init defaults; setGainDb() overrides below) ----
    {
        uint8_t g_default = (gain_db_to_es7210_register(0) & 0x0F) | 0x10;
        writeReg(0x43, g_default);
        writeReg(0x44, g_default);
        writeReg(0x45, g_default);
        writeReg(0x46, g_default);
    }
    // ---- MIC1-4 power on (driver writes 0x08 to each) ----
    writeReg(0x47, 0x08);
    writeReg(0x48, 0x08);
    writeReg(0x49, 0x08);
    writeReg(0x4A, 0x08);
    // ---- Sample-rate dividers (48 kHz, MCLK 12.288 MHz) ----
    writeReg(0x07, 0x20);                        // osr
    writeReg(0x02, 0x01 | (1 << 6) | (1 << 7));  // adc_div=1, doubler=1, dll=1 → 0xC1
    writeReg(0x04, 0x01);                        // lrck_h
    writeReg(0x05, 0x00);                        // lrck_l
    // ---- Power down DLL (driver leaves DLL off) ----
    writeReg(0x06, 0x04);
    // ---- MICBIAS + ADC + PGA power ----
    writeReg(0x4B, 0x0F);
    writeReg(0x4C, 0x0F);
    // ---- Enable device → run mode ----
    writeReg(0x00, 0x71);
    writeReg(0x00, 0x41);

    // Apply NVS-loaded gain to MIC1/MIC2 (lanes used by RLCD).
    setGainDb(0, gain_l_);
    setGainDb(1, gain_r_);
    setAlc(alc_);

    // Land in chip-idle (sleep) state — wake() flips back to run on first handle.
    writeReg(0x00, 0x32);

    ESP_LOGI(TAG, "Microphone initialized at 0x%02X", addr_);
    return true;
}

void Microphone::wake() {
    if (!codec_dev_) return;
    // Flip chip from idle (0x32) → run (0x71 → 0x41).
    writeReg(0x00, 0x71);
    writeReg(0x00, 0x41);
    bus_.setRxWarmupUntilUs(esp_timer_get_time() + 10'000);
    bus_.rxFlush();
    bus_.setRxConsumerActive(true);
}

void Microphone::sleep() {
    if (!codec_dev_) return;
    bus_.setRxConsumerActive(false);
    writeReg(0x00, 0x32);
}

int64_t Microphone::warmupRemainingUs() const {
    return bus_.rxWarmupRemainingUs();
}

// ---- Stubs retained from earlier task; will be replaced in Task 11. ----
Microphone::Handle& Microphone::Handle::operator=(Microphone::Handle&& o) noexcept {
    if (this != &o) {
        m_ = o.m_;
        o.m_ = nullptr;
    }
    return *this;
}
Microphone::Handle::~Handle() {}

Microphone::Handle Microphone::open() { return Handle{}; }
bool Microphone::capture(int16_t*, size_t, TickType_t) { return false; }
void Microphone::setGainDb(uint8_t channel, int8_t db) {
    if (channel == 0) gain_l_ = db; else gain_r_ = db;
}
int8_t Microphone::gainDb(uint8_t channel) const {
    return channel == 0 ? gain_l_ : gain_r_;
}
void Microphone::setAlc(bool on) { alc_ = on; }

void Microphone::retain() {}
void Microphone::release() {}

}  // namespace audio
