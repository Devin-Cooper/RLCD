#include "ssh_key_export.hpp"
#include "ssh_key_codec.hpp"

#include <cstdio>
#include <cstring>
#include <cerrno>

#include "esp_log.h"
#include "libssh/libssh.h"

static const char* TAG = "ssh_keys";
static constexpr int DISPLAY_COLS = 23;  // 8x12 font, 400/8.5 ≈ 47; 23 is the app convention for the smallest font

namespace ssh_keys {

static char priv_path_for(const KeyId& id, char* out, size_t cap) {
    std::snprintf(out, cap, "/littlefs/ssh_keys/%s", id.hex().c_str());
    return 0;
}

std::string wrapped_pubkey_line(KeyStore& store, const KeyId& id) {
    const KeyMeta* meta = store.find(id);
    if (!meta) return {};

    // Load the .pub file directly — we cached it at add time.
    char path[96];
    std::snprintf(path, sizeof(path), "/littlefs/ssh_keys/%s.pub", id.hex().c_str());
    FILE* f = std::fopen(path, "rb");
    if (!f) { ESP_LOGW(TAG, "wrapped_pubkey_line: fopen %s: %d", path, errno); return {}; }
    char line[600] = {};
    size_t n = std::fread(line, 1, sizeof(line) - 1, f);
    std::fclose(f);
    line[n] = '\0';
    // Strip trailing newline
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) { line[--n] = '\0'; }

    // Split into three fields: algo, b64, comment.
    char* p_sp1 = std::strchr(line, ' ');
    char* p_sp2 = p_sp1 ? std::strchr(p_sp1 + 1, ' ') : nullptr;

    std::string out;
    if (!p_sp1) { out = line; out.push_back('\n'); return out; }
    *p_sp1 = '\0';
    out.append(line);
    out.push_back('\n');

    const char* b64 = p_sp1 + 1;
    if (p_sp2) *p_sp2 = '\0';
    // Wrap b64 at 22 chars/line with 1-char indent ' ' so reader sees continuation
    size_t bl = std::strlen(b64);
    const int b64_cols = DISPLAY_COLS - 1;  // leave 1 space for indent
    for (size_t i = 0; i < bl; i += b64_cols) {
        out.push_back(' ');
        size_t chunk = std::min<size_t>(b64_cols, bl - i);
        out.append(b64 + i, chunk);
        out.push_back('\n');
    }
    if (p_sp2) {
        const char* comment = p_sp2 + 1;
        out.append(comment);
        out.push_back('\n');
    }
    return out;
}

} // namespace ssh_keys
