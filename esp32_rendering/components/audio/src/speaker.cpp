#include "speaker.hpp"
#include "audio_helpers.hpp"
#include <esp_log.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace audio::pa_ctrl {
    void init(gpio_num_t pin);
    void enable(gpio_num_t pin);
    void disable(gpio_num_t pin);
}

static const char* TAG = "speaker";
static constexpr const char* kNvsNs = "audio";
static constexpr const char* kNvsVol = "volume";
static constexpr const char* kNvsMute = "mute";

namespace {
    inline uint8_t vol_to_byte(uint8_t v_0_100) {
        if (v_0_100 == 0) return 0;
        return (uint8_t)((unsigned)v_0_100 * 256u / 100u - 1u);
    }
}

namespace audio {

Speaker::Speaker(audio_bus::AudioBus& bus, i2c_bsp::I2cMasterBus& i2c,
                 uint8_t addr_primary, uint8_t addr_fallback, gpio_num_t pa_ctrl)
    : bus_(bus), i2c_(i2c),
      addr_primary_(addr_primary), addr_fallback_(addr_fallback),
      pa_ctrl_(pa_ctrl) {}

Speaker::~Speaker() {
    if (codec_dev_) i2c_master_bus_rm_device(codec_dev_);
}

void Speaker::writeReg(uint8_t reg, uint8_t v) {
    if (!codec_dev_) return;
    i2c_.writeReg(codec_dev_, reg, &v, 1);
}
uint8_t Speaker::readReg(uint8_t reg) {
    if (!codec_dev_) return 0;
    uint8_t v = 0;
    i2c_.readReg(codec_dev_, reg, &v, 1);
    return v;
}

void Speaker::load() {
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t v = 60;
    if (nvs_get_u8(h, kNvsVol, &v) == ESP_OK) volume_ = v;
    uint8_t m = 0;
    if (nvs_get_u8(h, kNvsMute, &m) == ESP_OK) muted_ = (m != 0);
    nvs_close(h);
}

void Speaker::persist() {
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, kNvsVol, volume_);
    nvs_set_u8(h, kNvsMute, muted_ ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

bool Speaker::probeAddress() {
    auto try_addr = [&](uint8_t a) -> bool {
        i2c_master_dev_handle_t dev = nullptr;
        if (i2c_.addDevice(a, 100000, &dev) != ESP_OK) return false;
        uint8_t reg0 = 0;
        esp_err_t err = i2c_.readReg(dev, 0x00, &reg0, 1);
        if (err == ESP_OK) {
            codec_dev_ = dev;
            addr_actual_ = a;
            return true;
        }
        i2c_master_bus_rm_device(dev);
        return false;
    };
    if (try_addr(addr_primary_))  return true;
    if (try_addr(addr_fallback_)) return true;
    return false;
}

bool Speaker::init() {
    audio::pa_ctrl::init(pa_ctrl_);
    load();

    if (!probeAddress()) {
        ESP_LOGE(TAG, "ES8311 not detected at 0x%02X or 0x%02X", addr_primary_, addr_fallback_);
        return false;
    }
    ESP_LOGI(TAG, "ES8311 found at 0x%02X", addr_actual_);

    // ---- Reset (es8311.c: es8311_init reset block) ----
    writeReg(0x00, 0x1F);
    vTaskDelay(pdMS_TO_TICKS(20));
    writeReg(0x00, 0x00);
    writeReg(0x00, 0x80);  // power-on command

    // ---- Clock manager (mclk_from_mclk_pin=true, no inversions) ----
    // reg 0x01: enable all clocks, MCLK src = MCLK pin, no MCLK invert.
    writeReg(0x01, 0x3F);
    // reg 0x06: clear sclk inverter (bit 5).
    writeReg(0x06, readReg(0x06) & ~(1 << 5));

    // ---- Sample-rate dividers for (mclk=12.288 MHz, fs=48 kHz):
    //   pre_div=1, pre_multi=0, adc_div=1, dac_div=1, fs_mode=0,
    //   lrck_h=0x00, lrck_l=0xff, bclk_div=4, adc_osr=0x10, dac_osr=0x10
    {
        uint8_t r02 = readReg(0x02);
        r02 &= 0x07;                          // preserve low 3 bits
        r02 |= ((1 - 1) << 5);                // pre_div - 1 = 0
        r02 |= (0 << 3);                      // pre_multi = 0
        writeReg(0x02, r02);
    }
    writeReg(0x03, (0 << 6) | 0x10);          // fs_mode=0, adc_osr=0x10
    writeReg(0x04, 0x10);                     // dac_osr
    writeReg(0x05, ((1 - 1) << 4) | (1 - 1)); // adc_div=1, dac_div=1 → 0x00
    {
        uint8_t r06 = readReg(0x06);
        r06 &= 0xE0;
        r06 |= (4 - 1);                       // bclk_div=4 → low 5 bits = 3
        writeReg(0x06, r06);
    }
    {
        uint8_t r07 = readReg(0x07);
        r07 &= 0xC0;
        r07 |= 0x00;                          // lrck_h=0
        writeReg(0x07, r07);
    }
    writeReg(0x08, 0xFF);                     // lrck_l

    // ---- Audio format (slave, I²S, 16-bit in, 16-bit out) ----
    writeReg(0x00, readReg(0x00) & 0xBF);     // slave serial port (default-on)
    writeReg(0x09, 0x0C);                     // SDP-IN  16-bit ((3<<2)|0)
    writeReg(0x0A, 0x0C);                     // SDP-OUT 16-bit

    // ---- Power-up analog stack (es8311.c init tail) ----
    writeReg(0x0D, 0x01);                     // power up analog
    writeReg(0x0E, 0x02);                     // enable analog PGA + ADC modulator
    writeReg(0x12, 0x00);                     // power up DAC
    writeReg(0x13, 0x10);                     // enable HP-drive output
    writeReg(0x1C, 0x6A);                     // ADC EQ bypass + DC-offset cancel
    writeReg(0x37, 0x08);                     // bypass DAC equalizer

    // ---- Volume + soft-mute initial state ----
    {
        // es8311.c es8311_voice_volume_set: reg 0x32 = (vol*256/100)-1, or 0.
        uint8_t setpoint_dB = volume_0_100_to_codec_dB(volume_);
        writeReg(0x32, setpoint_dB == 0 ? 0
                       : (uint8_t)(((unsigned)setpoint_dB * 256u / 100u) - 1u));
    }
    {
        // soft-mute on: reg 0x31 |= BIT(6)|BIT(5)
        writeReg(0x31, readReg(0x31) | (1 << 6) | (1 << 5));
    }

    // ---- ADC PDN — RLCD does not use ES8311 mic input; R44 NC isolation. ----
    // reg 0x0E was just set to 0x02 (PGA+MOD enabled); add bits 5/6 to PDN them.
    writeReg(0x0E, readReg(0x0E) | (1 << 6) | (1 << 5));

    ESP_LOGI(TAG, "Speaker initialized");
    return true;
}

bool Speaker::play(const int16_t* pcm, size_t frames, TickType_t timeout) {
    if (!codec_dev_) return false;
    if (!awake_.load()) wake();
    size_t bytes = frames * sizeof(int16_t);
    size_t sent = bus_.write(pcm, bytes, timeout);
    return sent == bytes;
}

void Speaker::setVolume(uint8_t v) {
    if (v > 100) v = 100;
    volume_ = v;
    if (awake_.load() && codec_dev_) {
        writeReg(0x32, vol_to_byte(muted_ ? 0 : volume_));
    }
    persist();
}

void Speaker::mute(bool on) {
    muted_ = on;
    if (awake_.load() && codec_dev_) {
        writeReg(0x32, vol_to_byte(muted_ ? 0 : volume_));
    }
    persist();
}

void Speaker::wake() {
    if (awake_.load()) return;
    if (!codec_dev_) return;

    // Bring DAC analog stack out of PDN. Init powers up the common analog;
    // sleep() only PDNs DAC + VMID, so wake() inverts only those.
    writeReg(0x0D, readReg(0x0D) & ~((1 << 7) | (1 << 6) | (1 << 3) | (1 << 2)));
    {
        uint8_t r = readReg(0x0D);
        writeReg(0x0D, (r & ~0x03) | 0x02);  // VMIDSEL = 2 (normal vmid)
    }
    writeReg(0x12, readReg(0x12) & ~(1 << 1));   // PDN_DAC = 0

    vTaskDelay(pdMS_TO_TICKS(15));               // VMID settle

    audio::pa_ctrl::enable(pa_ctrl_);
    vTaskDelay(pdMS_TO_TICKS(8));                // NS4150B soft-start

    // Release soft-mute and ramp DAC volume from 0 → setpoint over 20 ms.
    writeReg(0x31, readReg(0x31) & ~((1 << 6) | (1 << 5)));
    uint8_t target = muted_ ? 0 : volume_;
    constexpr int kSteps = 20;
    for (int i = 1; i <= kSteps; ++i) {
        uint8_t step_v = (uint8_t)((unsigned)target * i / kSteps);
        writeReg(0x32, vol_to_byte(step_v));
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    awake_.store(true);
}

void Speaker::sleep() {
    if (!awake_.load()) return;
    if (!codec_dev_) return;

    uint8_t cur = muted_ ? 0 : volume_;
    constexpr int kSteps = 20;
    for (int i = kSteps - 1; i >= 0; --i) {
        uint8_t step_v = (uint8_t)((unsigned)cur * i / kSteps);
        writeReg(0x32, vol_to_byte(step_v));
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    // Soft-mute on.
    writeReg(0x31, readReg(0x31) | (1 << 6) | (1 << 5));
    audio::pa_ctrl::disable(pa_ctrl_);
    vTaskDelay(pdMS_TO_TICKS(5));
    // Power down DAC analog stage.
    writeReg(0x12, readReg(0x12) | (1 << 1));    // PDN_DAC = 1
    writeReg(0x0D, readReg(0x0D) | (1 << 7) | (1 << 6) | (1 << 3) | (1 << 2));
    writeReg(0x0D, readReg(0x0D) & ~0x03);       // VMIDSEL = 0 (vmid OFF)

    awake_.store(false);
}

}  // namespace audio
