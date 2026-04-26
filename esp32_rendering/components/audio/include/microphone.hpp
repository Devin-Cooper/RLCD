#pragma once

#include "audio_bus.hpp"
#include "i2c_bsp.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cstdint>

namespace audio {

class Microphone {
public:
    Microphone(audio_bus::AudioBus& bus, i2c_bsp::I2cMasterBus& i2c, uint8_t i2c_addr);
    ~Microphone();

    bool init();

    class Handle {
    public:
        Handle() = default;
        explicit Handle(Microphone* m) : m_(m) {}
        Handle(Handle&& o) noexcept : m_(o.m_) { o.m_ = nullptr; }
        Handle& operator=(Handle&& o) noexcept;
        ~Handle();
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        bool valid() const { return m_ != nullptr; }
    private:
        Microphone* m_ = nullptr;
    };

    Handle open();
    bool capture(int16_t* pcm_interleaved, size_t frames, TickType_t timeout);
    void setGainDb(uint8_t channel, int8_t db);
    int8_t gainDb(uint8_t channel) const;
    uint8_t channels() const { return 2; }
    void setAlc(bool on);

    static constexpr uint8_t kAddr = 0x40;

    int64_t warmupRemainingUs() const;

private:
    friend class Handle;
    audio_bus::AudioBus& bus_;
    i2c_bsp::I2cMasterBus& i2c_;
    uint8_t addr_;
    i2c_master_dev_handle_t codec_dev_ = nullptr;
    int8_t gain_l_ = 15, gain_r_ = 15;
    bool alc_ = false;
    SemaphoreHandle_t mtx_;
    int refcount_ = 0;

    void retain();
    void release();
    void wake();
    void sleep();
    void writeReg(uint8_t reg, uint8_t v);
    uint8_t readReg(uint8_t reg);
    void persist();
    void load();
};

}  // namespace audio
