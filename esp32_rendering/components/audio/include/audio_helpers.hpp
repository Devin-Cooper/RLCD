#pragma once

#include <cstddef>
#include <cstdint>

namespace audio { class Speaker; }

namespace audio {

// Generate a sine tone, blocking-write to speaker. Caller's buffer is internal.
// Returns when bytes are buffered, not when playback ends.
void beep(Speaker& spk, uint32_t hz, uint32_t ms, uint8_t volume_override = 255);

// RMS level in dBFS for one channel of stereo-interleaved 16-bit PCM.
// Returns -120.0f for silence (instead of -inf), 0.0f for full-scale.
float vuLevelDb(const int16_t* interleaved_pcm, size_t frames, uint8_t channel);

// Generate `samples` of a sine tone at `hz` into `out`. Returns samples written.
// Public so host tests + REPL can use it.
size_t generateSine(int16_t* out, size_t samples, uint32_t hz, uint32_t sample_rate, int16_t amplitude);

// Linear 0..100 -> ES8311 DAC dB (signed, -95..0). Pure function, host-testable.
int8_t volume_0_100_to_codec_dB(uint8_t v);

// dB (-6..+30) -> ES7210 PGA register value. Pure function, host-testable.
uint8_t gain_db_to_es7210_register(int8_t db);

}  // namespace audio
