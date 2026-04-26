#include "audio_helpers.hpp"
#include "speaker.hpp"

namespace audio {

void beep(Speaker& /*spk*/, uint32_t /*hz*/, uint32_t /*ms*/, uint8_t /*volume_override*/) {}

float vuLevelDb(const int16_t* /*pcm*/, size_t /*frames*/, uint8_t /*channel*/) { return -120.0f; }

size_t generateSine(int16_t* /*out*/, size_t /*samples*/, uint32_t /*hz*/, uint32_t /*sample_rate*/, int16_t /*amplitude*/) {
    return 0;
}

int8_t volume_0_100_to_codec_dB(uint8_t /*v*/) { return 0; }
uint8_t gain_db_to_es7210_register(int8_t /*db*/) { return 0; }

}  // namespace audio
