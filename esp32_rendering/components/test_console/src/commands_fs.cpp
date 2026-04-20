#include "test_console_response.hpp"
#include "esp_console.h"
#include "esp_log.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace test_console {

void registerFsWriteCommands();

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void emit_b64_chunk(const uint8_t* src, size_t n) {
    char enc[520];
    size_t j = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = src[i] << 16;
        size_t pad = 0;
        if (i + 1 < n) v |= src[i + 1] << 8; else pad++;
        if (i + 2 < n) v |= src[i + 2];     else pad++;
        enc[j++] = B64[(v >> 18) & 0x3f];
        enc[j++] = B64[(v >> 12) & 0x3f];
        enc[j++] = pad >= 2 ? '=' : B64[(v >> 6) & 0x3f];
        enc[j++] = pad >= 1 ? '=' : B64[ v       & 0x3f];
    }
    enc[j] = '\0';
    data("%s", enc);
}

static bool path_ok(const char* p) {
    return p && (strncmp(p, "/littlefs/", 10) == 0 || strncmp(p, "/sdcard/", 8) == 0);
}

// --- fs-ls <dir> ---
static int cmd_fs_ls(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-ls <dir>"); return 1; }
    if (!path_ok(argv[1])) { err(6, "path invalid (must start with /littlefs/ or /sdcard/)"); return 1; }
    DIR* d = opendir(argv[1]);
    if (!d) { err(2, "opendir: %d", errno); return 1; }
    struct dirent* e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[288];
        snprintf(full, sizeof(full), "%s/%s", argv[1], e->d_name);
        struct stat st;
        long sz = (stat(full, &st) == 0) ? (long)st.st_size : -1;
        data("%s %ld", e->d_name, sz);
    }
    closedir(d);
    ok("%s", "");
    return 0;
}

// --- fs-read <path> ---
static int cmd_fs_read(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-read <path>"); return 1; }
    if (!path_ok(argv[1])) { err(6, "path invalid"); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { err(7, "open: %d", errno); return 1; }
    uint8_t buf[384];
    size_t total = 0;
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        emit_b64_chunk(buf, n);
        total += n;
    }
    fclose(f);
    ok("%zu", total);
    return 0;
}

// --- fs-rm <path> ---
static int cmd_fs_rm(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-rm <path>"); return 1; }
    if (!path_ok(argv[1])) { err(6, "path invalid"); return 1; }
    if (remove(argv[1]) != 0) { err(3, "remove: %d", errno); return 1; }
    ok("%s", "");
    return 0;
}

// --- fs-mkdir <path> ---
static int cmd_fs_mkdir(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: fs-mkdir <path>"); return 1; }
    if (!path_ok(argv[1])) { err(6, "path invalid"); return 1; }
    if (mkdir(argv[1], 0755) != 0 && errno != EEXIST) { err(4, "mkdir: %d", errno); return 1; }
    ok("%s", "");
    return 0;
}

void registerFsCommands() {
    const esp_console_cmd_t cmds[] = {
        {"fs-ls",    "fs-ls <dir>",     nullptr, cmd_fs_ls,    nullptr, nullptr, nullptr},
        {"fs-read",  "fs-read <path>",  nullptr, cmd_fs_read,  nullptr, nullptr, nullptr},
        {"fs-rm",    "fs-rm <path>",    nullptr, cmd_fs_rm,    nullptr, nullptr, nullptr},
        {"fs-mkdir", "fs-mkdir <path>", nullptr, cmd_fs_mkdir, nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
    registerFsWriteCommands();
}

} // namespace test_console
