#include "test_console_response.hpp"
#include "esp_console.h"
#include "esp_timer.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

namespace test_console {

namespace {

struct WriteSlot {
    char     token[8];
    char     path[200];
    char     tmp_path[204];   // path + ".tmp" (4 bytes)
    FILE*    f;
    size_t   bytes;
    int64_t  last_activity_us;
};
WriteSlot s_slots[4] = {};

void slot_timeout_sweep() {
    int64_t now = esp_timer_get_time();
    for (auto& s : s_slots) {
        if (s.f && (now - s.last_activity_us) > 30LL * 1000000) {
            std::fclose(s.f); s.f = nullptr;
            std::remove(s.tmp_path);
            s.token[0] = '\0';
        }
    }
}

// Amendment E: sweep before lookup so stale slots don't linger.
WriteSlot* slot_by_token(const char* t) {
    slot_timeout_sweep();
    for (auto& s : s_slots) {
        if (s.f && std::strcmp(s.token, t) == 0) {
            s.last_activity_us = esp_timer_get_time();
            return &s;
        }
    }
    return nullptr;
}

int b64_decode_local(const char* in, uint8_t* out, size_t out_cap) {
    auto idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    size_t n = 0, len = std::strlen(in);
    for (size_t i = 0; i + 3 < len && in[i] != '='; i += 4) {
        int v0 = idx(in[i]),   v1 = idx(in[i+1]);
        int v2 = in[i+2] == '=' ? -2 : idx(in[i+2]);
        int v3 = in[i+3] == '=' ? -2 : idx(in[i+3]);
        if (v0 < 0 || v1 < 0) return -1;
        if (n >= out_cap) return -1;
        out[n++] = (v0 << 2) | (v1 >> 4);
        if (v2 == -2) break;
        if (v2 < 0) return -1;
        if (n >= out_cap) return -1;
        out[n++] = ((v1 & 0xF) << 4) | (v2 >> 2);
        if (v3 == -2) break;
        if (v3 < 0) return -1;
        if (n >= out_cap) return -1;
        out[n++] = ((v2 & 0x3) << 6) | v3;
    }
    return (int)n;
}

bool path_ok(const char* p) {
    return p && (std::strncmp(p, "/littlefs/", 10) == 0 ||
                 std::strncmp(p, "/sdcard/",  8) == 0);
}

} // anonymous

static int cmd_fs_write_begin(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-write-begin <path>"); return 1; }
    if (!path_ok(argv[1])) { err(6, "path invalid"); return 1; }
    slot_timeout_sweep();
    WriteSlot* slot = nullptr;
    for (auto& s : s_slots) if (!s.f) { slot = &s; break; }
    if (!slot) { err(8, "all slots busy"); return 1; }
    std::strncpy(slot->path, argv[1], sizeof(slot->path) - 1);
    slot->path[sizeof(slot->path) - 1] = '\0';
    std::snprintf(slot->tmp_path, sizeof(slot->tmp_path), "%s.tmp", slot->path);
    slot->f = std::fopen(slot->tmp_path, "wb");
    if (!slot->f) { err(3, "fopen: %d", errno); return 1; }
    static uint32_t seed = 0x13370001;
    seed = seed * 1103515245 + 12345;
    std::snprintf(slot->token, sizeof(slot->token), "%06x",
                  (unsigned)(seed & 0xFFFFFF));
    slot->bytes = 0;
    slot->last_activity_us = esp_timer_get_time();
    ok("%s", slot->token);
    return 0;
}

static int cmd_fs_write_chunk(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: fs-write-chunk <token> <base64>"); return 1; }
    WriteSlot* slot = slot_by_token(argv[1]);
    if (!slot) { err(9, "bad token"); return 1; }
    uint8_t dec[4096];
    int n = b64_decode_local(argv[2], dec, sizeof(dec));
    if (n < 0) { err(10, "base64 decode"); return 1; }
    if (slot->bytes + (size_t)n > 65536) { err(11, "size exceeded 64KB"); return 1; }
    if ((int)std::fwrite(dec, 1, n, slot->f) != n) {
        err(3, "fwrite: %d", errno); return 1;
    }
    slot->bytes += (size_t)n;
    ok("%zu", slot->bytes);
    return 0;
}

static int cmd_fs_write_commit(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-write-commit <token>"); return 1; }
    WriteSlot* slot = slot_by_token(argv[1]);
    if (!slot) { err(9, "bad token"); return 1; }
    if (slot->bytes == 0) { err(4, "empty file"); return 1; }
    std::fclose(slot->f); slot->f = nullptr;
    if (std::rename(slot->tmp_path, slot->path) != 0) {
        err(12, "rename: %d", errno); return 1;
    }
    slot->token[0] = '\0';
    ok("%s", "");
    return 0;
}

static int cmd_fs_write_abort(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-write-abort <token>"); return 1; }
    WriteSlot* slot = slot_by_token(argv[1]);
    if (!slot) { err(9, "bad token"); return 1; }
    std::fclose(slot->f); slot->f = nullptr;
    std::remove(slot->tmp_path);
    slot->token[0] = '\0';
    ok("%s", "");
    return 0;
}

void registerFsWriteCommands() {
    const esp_console_cmd_t cmds[] = {
        {"fs-write-begin",  "fs-write-begin <path> -> token",  nullptr, cmd_fs_write_begin,  nullptr, nullptr, nullptr},
        {"fs-write-chunk",  "fs-write-chunk <token> <base64>", nullptr, cmd_fs_write_chunk,  nullptr, nullptr, nullptr},
        {"fs-write-commit", "fs-write-commit <token>",         nullptr, cmd_fs_write_commit, nullptr, nullptr, nullptr},
        {"fs-write-abort",  "fs-write-abort <token>",          nullptr, cmd_fs_write_abort,  nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
