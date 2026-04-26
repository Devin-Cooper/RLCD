#include "audio_helpers.hpp"
#include "speaker.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Note: pure helpers (generateSine, vuLevelDb, volume_0_100_to_codec_dB,
// gain_db_to_es7210_register) live in audio_helpers_pure.cpp so host tests
// can link them without speaker.hpp's I²C/I²S transitive dependencies.

namespace audio {

void beep(Speaker& spk, uint32_t hz, uint32_t ms, uint8_t volume_override) {
    constexpr size_t kSampleRate = 48000;
    size_t samples = (size_t)kSampleRate * ms / 1000;
    if (samples == 0) return;

    uint8_t prev_vol = spk.volume();
    if (volume_override != 255) spk.setVolume(volume_override);

    constexpr size_t kChunk = 1024;
    static int16_t buf[kChunk];
    size_t written = 0;
    while (written < samples) {
        size_t n = std::min(kChunk, samples - written);
        // Generate the chunk with phase continuity.
        const double w = 2.0 * M_PI * (double)hz / (double)kSampleRate;
        for (size_t i = 0; i < n; ++i) {
            buf[i] = (int16_t)(16000.0 * std::sin(w * (double)(written + i)));
        }
        spk.play(buf, n, pdMS_TO_TICKS(500));
        written += n;
    }

    if (volume_override != 255) spk.setVolume(prev_vol);
}

}  // namespace audio
