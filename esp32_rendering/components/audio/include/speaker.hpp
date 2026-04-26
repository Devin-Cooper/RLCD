#pragma once

#include "audio_bus.hpp"
#include "i2c_bsp.hpp"
#include <driver/gpio.h>
#include <atomic>
#include <cstdint>

namespace audio {

class Speaker {
public:
    Speaker(audio_bus::AudioBus& bus, i2c_bsp::I2cMasterBus& i2c,
            uint8_t i2c_addr_primary, uint8_t i2c_addr_fallback,
            gpio_num_t pa_ctrl);
    ~Speaker();

    bool init();
    bool play(const int16_t* pcm, size_t frames, TickType_t timeout);
    void setVolume(uint8_t vol_0_100);
    uint8_t volume() const { return volume_; }
    void mute(bool on);
    bool isMuted() const { return muted_; }
    void wake();
    void sleep();
    bool isAwake() const { return awake_; }

    static constexpr uint8_t kAddrPrimary  = 0x18;
    static constexpr uint8_t kAddrFallback = 0x19;

private:
    audio_bus::AudioBus& bus_;
    i2c_bsp::I2cMasterBus& i2c_;
    uint8_t addr_primary_;
    uint8_t addr_fallback_;
    uint8_t addr_actual_ = 0;
    gpio_num_t pa_ctrl_;
    uint8_t volume_ = 60;
    bool muted_ = false;
    std::atomic<bool> awake_{false};

    void* codec_handle_ = nullptr;  // espressif/es8311 handle
    void writeReg(uint8_t reg, uint8_t v);
    uint8_t readReg(uint8_t reg);
    bool probeAddress();
    void persist();
    void load();
};

}  // namespace audio
