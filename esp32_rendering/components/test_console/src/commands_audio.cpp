#include "test_console_response.hpp"
#include "test_console_context.hpp"
#include "speaker.hpp"
#include "microphone.hpp"
#include "audio_helpers.hpp"

#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace test_console {

static bool path_ok_audio(const char* p) {
    return p && (std::strncmp(p, "/littlefs/", 10) == 0 || std::strncmp(p, "/sdcard/", 8) == 0);
}

// audio_tone <hz> <ms> [vol]
static int cmd_audio_tone(int argc, char** argv) {
    if (argc < 3 || argc > 4) { err(1, "usage: audio_tone <hz> <ms> [vol]"); return 1; }
    Context* ctx = getContext();
    if (!ctx) { err(2, "no context"); return 1; }
    uint32_t hz = (uint32_t)std::atoi(argv[1]);
    uint32_t ms = (uint32_t)std::atoi(argv[2]);
    uint8_t  vo = (argc == 4) ? (uint8_t)std::atoi(argv[3]) : 255;
    audio::beep(ctx->speaker, hz, ms, vo);
    ok("%lu %lu", (unsigned long)hz, (unsigned long)ms);
    return 0;
}

// audio_record <seconds> <path>
static int cmd_audio_record(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: audio_record <seconds> <path>"); return 1; }
    Context* ctx = getContext();
    if (!ctx) { err(2, "no context"); return 1; }
    if (!path_ok_audio(argv[2])) {
        err(6, "path invalid (must start with /littlefs/ or /sdcard/)");
        return 1;
    }
    int seconds = std::atoi(argv[1]);
    if (seconds <= 0) { err(3, "seconds must be > 0"); return 1; }
    const char* path = argv[2];

    auto h = ctx->microphone.open();
    if (!h.valid()) { err(4, "microphone open failed"); return 1; }

    FILE* f = std::fopen(path, "wb");
    if (!f) { err(7, "fopen: %d", errno); return 1; }

    constexpr size_t kFrames = 480;
    static int16_t buf[kFrames * 2];
    size_t total_frames = (size_t)seconds * 48000;
    size_t got = 0;
    while (got < total_frames) {
        size_t want = std::min(kFrames, total_frames - got);
        if (!ctx->microphone.capture(buf, want, pdMS_TO_TICKS(500))) break;
        size_t wrote = std::fwrite(buf, sizeof(int16_t), want * 2, f);
        if (wrote != want * 2) { std::fclose(f); err(8, "fwrite short: %zu", wrote); return 1; }
        got += want;
    }
    std::fclose(f);
    ok("%zu", got);
    return 0;
}

// audio_loopback <seconds>
static int cmd_audio_loopback(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: audio_loopback <seconds>"); return 1; }
    Context* ctx = getContext();
    if (!ctx) { err(2, "no context"); return 1; }
    int seconds = std::atoi(argv[1]);
    if (seconds <= 0) { err(3, "seconds must be > 0"); return 1; }

    auto h = ctx->microphone.open();
    if (!h.valid()) { err(4, "microphone open failed"); return 1; }

    constexpr size_t kFrames = 480;
    static int16_t buf[kFrames * 2];
    static int16_t mono[kFrames];
    size_t total_frames = (size_t)seconds * 48000;
    size_t got = 0;
    while (got < total_frames) {
        size_t want = std::min(kFrames, total_frames - got);
        if (!ctx->microphone.capture(buf, want, pdMS_TO_TICKS(500))) break;
        for (size_t i = 0; i < want; ++i) {
            mono[i] = (int16_t)((buf[i * 2] + buf[i * 2 + 1]) / 2);
        }
        ctx->speaker.play(mono, want, pdMS_TO_TICKS(500));
        got += want;
    }
    ok("%zu", got);
    return 0;
}

// audio_status
static int cmd_audio_status(int /*argc*/, char** /*argv*/) {
    Context* ctx = getContext();
    if (!ctx) { err(2, "no context"); return 1; }
    data("vol=%u mute=%d gainL=%+d gainR=%+d awake=%d",
         (unsigned)ctx->speaker.volume(),
         (int)ctx->speaker.isMuted(),
         (int)ctx->microphone.gainDb(0),
         (int)ctx->microphone.gainDb(1),
         (int)ctx->speaker.isAwake());
    ok("%s", "");
    return 0;
}

void registerAudioCommands() {
    const esp_console_cmd_t cmds[] = {
        {"audio_tone",     "audio_tone <hz> <ms> [vol]",     nullptr, cmd_audio_tone,     nullptr, nullptr, nullptr},
        {"audio_record",   "audio_record <seconds> <path>",  nullptr, cmd_audio_record,   nullptr, nullptr, nullptr},
        {"audio_loopback", "audio_loopback <seconds>",       nullptr, cmd_audio_loopback, nullptr, nullptr, nullptr},
        {"audio_status",   "audio_status",                   nullptr, cmd_audio_status,   nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

}  // namespace test_console
