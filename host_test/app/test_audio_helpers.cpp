#include <catch2/catch_test_macros.hpp>
#include "audio_helpers.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

TEST_CASE("generateSine length + amplitude", "[audio_helpers]") {
    std::vector<int16_t> out(48);
    size_t n = audio::generateSine(out.data(), out.size(), 1000, 48000, 16000);
    REQUIRE(n == 48);
    int16_t peak = 0;
    for (auto v : out) {
        int abs_v = std::abs((int)v);
        if (abs_v > peak) peak = (int16_t)abs_v;
    }
    REQUIRE(peak > 14000);
    REQUIRE(peak <= 16000);
}

TEST_CASE("generateSine zero sample rate yields zero samples", "[audio_helpers]") {
    int16_t out[8] = {0};
    size_t n = audio::generateSine(out, 8, 1000, 0, 16000);
    REQUIRE(n == 0);
}

TEST_CASE("vuLevelDb silence is -120", "[audio_helpers]") {
    std::vector<int16_t> z(960, 0);  // 480 frames stereo
    REQUIRE(audio::vuLevelDb(z.data(), 480, 0) <= -119.0f);
    REQUIRE(audio::vuLevelDb(z.data(), 480, 1) <= -119.0f);
}

TEST_CASE("vuLevelDb full-scale square is ~0 dBFS, opposite channel silent", "[audio_helpers]") {
    std::vector<int16_t> sq(960);
    for (size_t i = 0; i < 480; ++i) {
        sq[i * 2]     = (i & 1) ? 32767 : -32767;
        sq[i * 2 + 1] = 0;
    }
    float l = audio::vuLevelDb(sq.data(), 480, 0);
    float r = audio::vuLevelDb(sq.data(), 480, 1);
    REQUIRE(l > -1.0f);
    REQUIRE(l <= 0.0f);
    REQUIRE(r <= -119.0f);
}

TEST_CASE("vuLevelDb out-of-range channel returns -120", "[audio_helpers]") {
    std::vector<int16_t> z(960, 100);
    REQUIRE(audio::vuLevelDb(z.data(), 480, 2) <= -119.0f);
}

TEST_CASE("volume_0_100_to_codec_dB endpoints", "[audio_helpers]") {
    REQUIRE(audio::volume_0_100_to_codec_dB(0)   == -95);
    REQUIRE(audio::volume_0_100_to_codec_dB(100) == 0);
    REQUIRE(audio::volume_0_100_to_codec_dB(200) == 0);  // clamps high
}

TEST_CASE("volume_0_100_to_codec_dB midpoint is monotonic", "[audio_helpers]") {
    int8_t prev = audio::volume_0_100_to_codec_dB(1);
    for (uint8_t v = 2; v <= 99; ++v) {
        int8_t cur = audio::volume_0_100_to_codec_dB(v);
        REQUIRE(cur >= prev);
        prev = cur;
    }
}

TEST_CASE("gain_db_to_es7210_register clamping", "[audio_helpers]") {
    REQUIRE(audio::gain_db_to_es7210_register(-10) == 0);
    REQUIRE(audio::gain_db_to_es7210_register(-6)  == 0);
    REQUIRE(audio::gain_db_to_es7210_register(-1)  == 0);
    REQUIRE(audio::gain_db_to_es7210_register(0)   == 0);
    REQUIRE(audio::gain_db_to_es7210_register(15)  == 15);
    REQUIRE(audio::gain_db_to_es7210_register(30)  == 30);
    REQUIRE(audio::gain_db_to_es7210_register(40)  == 30);  // clamps high
}
