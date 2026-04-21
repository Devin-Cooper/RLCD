#include "ssh_key_export.hpp"

#include <cstdio>

#include "esp_log.h"
#include "qrcodegen.h"

// Interact with the 1-bit framebuffer via the project's onebit API.
#include "1bit/core/framebuffer.hpp"

static const char* TAG = "ssh_keys";

namespace ssh_keys {

int render_qr_to_framebuffer(KeyStore& store, const KeyId& id,
                             void* framebuffer_opaque,
                             int origin_x, int origin_y) {
    if (!store.contains(id)) {
        ESP_LOGE(TAG, "qr: key not in store: %s", id.hex().c_str());
        return 0;
    }

    // Build pubkey line from cached .pub
    char path[96];
    std::snprintf(path, sizeof(path), "/littlefs/ssh_keys/%s.pub", id.hex().c_str());
    char line[768] = {};
    FILE* f = std::fopen(path, "rb");
    if (!f) { ESP_LOGE(TAG, "qr: fopen %s failed", path); return 0; }
    size_t n = std::fread(line, 1, sizeof(line) - 1, f);
    std::fclose(f);
    if (n == sizeof(line) - 1) {
        ESP_LOGE(TAG, "qr: pubkey line >= %zu B, truncated — refusing to encode", sizeof(line) - 1);
        return 0;
    }
    line[n] = '\0';
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';

    uint8_t qr_buf[qrcodegen_BUFFER_LEN_FOR_VERSION(17)];
    uint8_t tmp_buf[qrcodegen_BUFFER_LEN_FOR_VERSION(17)];
    bool ok = qrcodegen_encodeText(line,
                                   tmp_buf,
                                   qr_buf,
                                   qrcodegen_Ecc_LOW,
                                   qrcodegen_VERSION_MIN,
                                   17,
                                   qrcodegen_Mask_AUTO,
                                   true);
    if (!ok) { ESP_LOGE(TAG, "qrcodegen_encodeText failed"); return 0; }

    int modules = qrcodegen_getSize(qr_buf);
    int scale = select_qr_scale(modules);
    if (scale == 0) { ESP_LOGE(TAG, "qr: too large (modules=%d)", modules); return 0; }

    auto* fb = static_cast<onebit::IFramebuffer*>(framebuffer_opaque);
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            bool dark = qrcodegen_getModule(qr_buf, x, y);
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    fb->setPixel(origin_x + x * scale + dx,
                                 origin_y + y * scale + dy,
                                 dark);
                }
            }
        }
    }
    return scale;
}

} // namespace ssh_keys
