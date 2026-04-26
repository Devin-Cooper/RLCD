#include "audio_helpers.hpp"
#include "speaker.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace audio {

size_t generateSine(int16_t* out, size_t samples, uint32_t hz, uint32_t sample_rate, int16_t amplitude) {
    if (sample_rate == 0) return 0;
    const double w = 2.0 * M_PI * (double)hz / (double)sample_rate;
    for (size_t i = 0; i < samples; ++i) {
        out[i] = (int16_t)((double)amplitude * std::sin(w * (double)i));
    }
    return samples;
}

float vuLevelDb(const int16_t* pcm, size_t frames, uint8_t channel) {
    if (frames == 0) return -120.0f;
    if (channel > 1) return -120.0f;
    double sumsq = 0.0;
    for (size_t i = 0; i < frames; ++i) {
        double s = (double)pcm[i * 2 + channel] / 32768.0;
        sumsq += s * s;
    }
    double rms = std::sqrt(sumsq / (double)frames);
    if (rms < 1e-6) return -120.0f;
    double db = 20.0 * std::log10(rms);
    if (db < -120.0f) db = -120.0f;
    if (db > 0.0f) db = 0.0f;
    return (float)db;
}

int8_t volume_0_100_to_codec_dB(uint8_t v) {
    if (v == 0) return -95;
    if (v >= 100) return 0;
    // Linear 1..99 -> -60..0 dB (skip the bottom 35 dB which would be inaudible)
    int8_t db = (int8_t)(((int)v - 1) * 60 / 99 - 60);
    return db;
}

uint8_t gain_db_to_es7210_register(int8_t db) {
    if (db < -6)  db = -6;
    if (db > 30)  db = 30;
    // ES7210 PGA register: 0x00 = 0 dB; each step = +1 dB up to +30; -6 dB
    // is encoded as a special value. Per ES7210 datasheet table 4-6:
    // register field MIC_GAIN: 0x00..0x1E -> 0..+30 dB. -6 dB selects the
    // attenuator path (separate bit). Implementation here returns just the
    // gain field; the attenuator bit is set elsewhere.
    if (db < 0) return 0x00;
    return (uint8_t)db;
}

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
