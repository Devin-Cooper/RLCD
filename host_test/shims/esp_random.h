/* host_test shim for <esp_random.h>. Uses std::random_device-seeded Mersenne
 * Twister for high-quality randomness — sufficient for 10k-draw uniqueness
 * tests in KeyId::random(). */
#ifndef RLCD_HOST_SHIM_ESP_RANDOM_H
#define RLCD_HOST_SHIM_ESP_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <random>

inline std::mt19937_64& _rlcd_host_rng() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    return rng;
}

extern "C" {
#endif

static inline void esp_fill_random(void* buf, size_t len) {
#ifdef __cplusplus
    auto& rng = _rlcd_host_rng();
    uint8_t* p = (uint8_t*)buf;
    size_t i = 0;
    while (i + 8 <= len) {
        uint64_t v = rng();
        for (int j = 0; j < 8; ++j) p[i++] = (uint8_t)(v >> (8 * j));
    }
    while (i < len) {
        uint64_t v = rng();
        p[i++] = (uint8_t)v;
    }
#else
    /* Non-C++ callers: fallback to a simple LCG seeded from address. */
    static uint64_t s = 0x9E3779B97F4A7C15ULL;
    uint8_t* p = (uint8_t*)buf;
    for (size_t i = 0; i < len; ++i) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        p[i] = (uint8_t)(s >> 56);
    }
#endif
}

static inline uint32_t esp_random(void) {
    uint32_t v = 0;
    esp_fill_random(&v, sizeof(v));
    return v;
}

#ifdef __cplusplus
}
#endif

#endif
