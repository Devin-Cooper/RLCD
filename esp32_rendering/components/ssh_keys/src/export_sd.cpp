#include "ssh_key_export.hpp"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"

static const char* TAG = "ssh_keys";
static constexpr const char* SD_EXPORT_DIR = "/sdcard/export";

namespace ssh_keys {

bool export_sd(KeyStore& store, const KeyId& id, char* out_err, size_t cap) {
    const KeyMeta* meta = store.find(id);
    if (!meta) { std::snprintf(out_err, cap, "key not found"); return false; }

    // Source: /littlefs/ssh_keys/<id>.pub
    char src[96];
    std::snprintf(src, sizeof(src), "/littlefs/ssh_keys/%s.pub", id.hex().c_str());
    FILE* fs = std::fopen(src, "rb");
    if (!fs) { std::snprintf(out_err, cap, "src open: %d", errno); return false; }
    char buf[1024] = {};
    size_t n = std::fread(buf, 1, sizeof(buf), fs);
    std::fclose(fs);
    if (n == 0) { std::snprintf(out_err, cap, "src empty"); return false; }

    // Ensure destination directory
    struct stat st;
    if (stat(SD_EXPORT_DIR, &st) != 0) {
        if (mkdir(SD_EXPORT_DIR, 0755) != 0 && errno != EEXIST) {
            std::snprintf(out_err, cap, "mkdir: %d", errno); return false;
        }
    }
    char dst[128], tmp[132];
    std::snprintf(dst, sizeof(dst), "%s/%s.pub",     SD_EXPORT_DIR, meta->name);
    std::snprintf(tmp, sizeof(tmp), "%s/%s.pub.tmp", SD_EXPORT_DIR, meta->name);
    FILE* fd = std::fopen(tmp, "wb");
    if (!fd) { std::snprintf(out_err, cap, "dst open: %d", errno); return false; }
    size_t w = std::fwrite(buf, 1, n, fd);
    std::fclose(fd);
    if (w != n) { unlink(tmp); std::snprintf(out_err, cap, "short write"); return false; }
    if (rename(tmp, dst) != 0) {
        unlink(tmp);
        std::snprintf(out_err, cap, "rename: %d", errno);
        return false;
    }
    ESP_LOGI(TAG, "exported to %s", dst);
    return true;
}

} // namespace ssh_keys
