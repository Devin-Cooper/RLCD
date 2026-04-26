#include "speaker.hpp"

namespace audio {

Speaker::Speaker(audio_bus::AudioBus& bus, i2c_bsp::I2cMasterBus& i2c,
                 uint8_t addr_primary, uint8_t addr_fallback, gpio_num_t pa_ctrl)
    : bus_(bus), i2c_(i2c),
      addr_primary_(addr_primary), addr_fallback_(addr_fallback),
      pa_ctrl_(pa_ctrl) {}

Speaker::~Speaker() {}

bool Speaker::init() { return false; }
bool Speaker::play(const int16_t*, size_t, TickType_t) { return false; }
void Speaker::setVolume(uint8_t v) { volume_ = v; }
void Speaker::mute(bool on) { muted_ = on; }
void Speaker::wake() {}
void Speaker::sleep() {}

void Speaker::writeReg(uint8_t, uint8_t) {}
uint8_t Speaker::readReg(uint8_t) { return 0; }
bool Speaker::probeAddress() { return false; }
void Speaker::persist() {}
void Speaker::load() {}

}  // namespace audio
