#include "microphone.hpp"

namespace audio {

Microphone::Handle& Microphone::Handle::operator=(Microphone::Handle&& o) noexcept {
    if (this != &o) {
        m_ = o.m_;
        o.m_ = nullptr;
    }
    return *this;
}
Microphone::Handle::~Handle() {}

Microphone::Microphone(audio_bus::AudioBus& bus, i2c_bsp::I2cMasterBus& i2c, uint8_t i2c_addr)
    : bus_(bus), i2c_(i2c), addr_(i2c_addr) {
    mtx_ = xSemaphoreCreateMutex();
}
Microphone::~Microphone() {
    if (mtx_) vSemaphoreDelete(mtx_);
}

bool Microphone::init() { return false; }
Microphone::Handle Microphone::open() { return Handle{}; }
bool Microphone::capture(int16_t*, size_t, TickType_t) { return false; }
void Microphone::setGainDb(uint8_t channel, int8_t db) {
    if (channel == 0) gain_l_ = db; else gain_r_ = db;
}
int8_t Microphone::gainDb(uint8_t channel) const {
    return channel == 0 ? gain_l_ : gain_r_;
}
void Microphone::setAlc(bool on) { alc_ = on; }
int64_t Microphone::warmupRemainingUs() const { return 0; }

void Microphone::retain() {}
void Microphone::release() {}
void Microphone::wake() {}
void Microphone::sleep() {}
void Microphone::writeReg(uint8_t, uint8_t) {}
uint8_t Microphone::readReg(uint8_t) { return 0; }
void Microphone::persist() {}
void Microphone::load() {}

}  // namespace audio
