#include "test_console_response.hpp"
#include "esp_console.h"
#include "esp_random.h"
#include "esp_timer.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <sys/stat.h>
#include <unistd.h>

namespace test_console {

namespace {

constexpr int kMaxSlots = 4;
constexpr int64_t kSlotTimeoutUs = 30 * 1000 * 1000;  // 30s

struct UploadSlot {
    bool in_use = false;
    char token[17] = {};     // 16 hex + NUL
    char path[128] = {};
    FILE* fp = nullptr;
    uint32_t expected = 0;
    uint32_t written = 0;
    uint32_t next_seq = 0;
    int64_t last_activity_us = 0;
};

UploadSlot g_slots[kMaxSlots];

void close_slot(UploadSlot& s) {
    if (s.fp) { fclose(s.fp); s.fp = nullptr; }
    // Best-effort unlink of partial file on abandoned/closed slot.
    if (s.path[0]) unlink(s.path);
    s.in_use = false;
    s.token[0] = '\0';
    s.path[0] = '\0';
    s.expected = s.written = s.next_seq = 0;
    s.last_activity_us = 0;
}

// Amendment E: sweep stale slots on every lookup path.
void sweep_stale() {
    int64_t now = esp_timer_get_time();
    for (auto& s : g_slots) {
        if (s.in_use && (now - s.last_activity_us) > kSlotTimeoutUs) {
            close_slot(s);
        }
    }
}

UploadSlot* slot_by_token(const char* token) {
    sweep_stale();
    for (auto& s : g_slots) {
        if (s.in_use && std::strcmp(s.token, token) == 0) {
            s.last_activity_us = esp_timer_get_time();
            return &s;
        }
    }
    return nullptr;
}

UploadSlot* slot_alloc() {
    sweep_stale();
    for (auto& s : g_slots) {
        if (!s.in_use) return &s;
    }
    return nullptr;
}

void make_token(char out[17]) {
    uint32_t a = esp_random();
    uint32_t b = esp_random();
    std::snprintf(out, 17, "%08lx%08lx",
                  (unsigned long)a, (unsigned long)b);
}

bool path_ok(const char* p) {
    return p && (std::strncmp(p, "/littlefs/", 10) == 0 ||
                 std::strncmp(p, "/sdcard/",  8) == 0);
}

int b64_decode(const char* in, uint8_t* out, size_t out_cap) {
    auto idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    size_t len = std::strlen(in);
    if (len % 4 != 0) return -1;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 4) {
        int a = idx(in[i]);
        int b = idx(in[i + 1]);
        int c = (in[i + 2] == '=') ? 0 : idx(in[i + 2]);
        int d = (in[i + 3] == '=') ? 0 : idx(in[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return -1;
        uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;
        if (o < out_cap) out[o++] = (v >> 16) & 0xff;
        if (in[i + 2] != '=' && o < out_cap) out[o++] = (v >> 8) & 0xff;
        if (in[i + 3] != '=' && o < out_cap) out[o++] = v & 0xff;
    }
    return (int)o;
}

} // anonymous

static int cmd_fs_write_begin(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: fs-write-begin <path> <expected_size>"); return 1; }
    if (!path_ok(argv[1])) { err(6, "path invalid"); return 1; }
    char* endp = nullptr;
    unsigned long sz = std::strtoul(argv[2], &endp, 10);
    if (!endp || *endp != '\0') { err(1, "bad size"); return 1; }

    UploadSlot* s = slot_alloc();
    if (!s) { err(8, "no free slots"); return 1; }

    FILE* f = std::fopen(argv[1], "wb");
    if (!f) { err(7, "open: %d", errno); return 1; }

    s->in_use = true;
    make_token(s->token);
    std::strncpy(s->path, argv[1], sizeof(s->path) - 1);
    s->path[sizeof(s->path) - 1] = '\0';
    s->fp = f;
    s->expected = (uint32_t)sz;
    s->written = 0;
    s->next_seq = 0;
    s->last_activity_us = esp_timer_get_time();
    ok("%s", s->token);
    return 0;
}

static int cmd_fs_write_chunk(int argc, char** argv) {
    if (argc != 4) { err(1, "usage: fs-write-chunk <token> <seq> <b64>"); return 1; }
    UploadSlot* s = slot_by_token(argv[1]);
    if (!s) { err(9, "bad token"); return 1; }
    char* endp = nullptr;
    unsigned long seq = std::strtoul(argv[2], &endp, 10);
    if (!endp || *endp != '\0') { err(1, "bad seq"); return 1; }
    if ((uint32_t)seq != s->next_seq) {
        err(10, "seq mismatch: got %lu want %lu",
            (unsigned long)seq, (unsigned long)s->next_seq);
        return 1;
    }
    uint8_t buf[768];
    int n = b64_decode(argv[3], buf, sizeof(buf));
    if (n < 0) { err(11, "bad b64"); return 1; }
    if (s->written + (uint32_t)n > s->expected) {
        err(12, "over-size: would write %u want %u",
            (unsigned)(s->written + (uint32_t)n), (unsigned)s->expected);
        return 1;
    }
    size_t w = std::fwrite(buf, 1, (size_t)n, s->fp);
    if ((int)w != n) { err(13, "fwrite short: %zu/%d", w, n); return 1; }
    s->written += (uint32_t)n;
    s->next_seq++;
    ok("%u", (unsigned)s->written);
    return 0;
}

static int cmd_fs_write_end(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-write-end <token>"); return 1; }
    UploadSlot* s = slot_by_token(argv[1]);
    if (!s) { err(9, "bad token"); return 1; }
    if (s->written != s->expected) {
        err(14, "size mismatch: wrote %u expected %u",
            (unsigned)s->written, (unsigned)s->expected);
        // Drop partial; close_slot() unlinks.
        close_slot(*s);
        return 1;
    }
    // Success: close file, clear slot WITHOUT unlinking the final file.
    if (s->fp) { std::fclose(s->fp); s->fp = nullptr; }
    s->in_use = false;
    s->token[0] = '\0';
    s->path[0] = '\0';
    s->expected = s->written = s->next_seq = 0;
    s->last_activity_us = 0;
    ok("%s", "");
    return 0;
}

void registerFsWriteCommands() {
    const esp_console_cmd_t cmds[] = {
        {"fs-write-begin", "fs-write-begin <path> <expected>",  nullptr, cmd_fs_write_begin, nullptr, nullptr, nullptr},
        {"fs-write-chunk", "fs-write-chunk <token> <seq> <b64>", nullptr, cmd_fs_write_chunk, nullptr, nullptr, nullptr},
        {"fs-write-end",   "fs-write-end <token>",               nullptr, cmd_fs_write_end,   nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
