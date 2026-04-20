#include "config_store_nvs.hpp"
#include <nvs.h>
#include <esp_log.h>
#include <cstdio>
#include <cstring>

static const char* TAG = "config_store_nvs";
static const char* NVS_NS_SERVERS = "servers";
static const char* NVS_NS_SETTINGS = "app_settings";

namespace sdcard {

namespace {

esp_err_t nvsSetStr(nvs_handle_t h, const char* prefix, int idx,
                    const char* val) {
    char k[16];
    snprintf(k, sizeof(k), "%s_%d", prefix, idx);
    return nvs_set_str(h, k, val ? val : "");
}

esp_err_t nvsGetStr(nvs_handle_t h, const char* prefix, int idx,
                    char* out, size_t out_cap) {
    char k[16];
    snprintf(k, sizeof(k), "%s_%d", prefix, idx);
    size_t len = out_cap;
    esp_err_t e = nvs_get_str(h, k, out, &len);
    if (e != ESP_OK && out_cap > 0) out[0] = '\0';
    return e;
}

esp_err_t nvsSetU16(nvs_handle_t h, const char* prefix, int idx, uint16_t v) {
    char k[16]; snprintf(k, sizeof(k), "%s_%d", prefix, idx);
    return nvs_set_u16(h, k, v);
}

esp_err_t nvsSetU8(nvs_handle_t h, const char* prefix, int idx, uint8_t v) {
    char k[16]; snprintf(k, sizeof(k), "%s_%d", prefix, idx);
    return nvs_set_u8(h, k, v);
}

esp_err_t nvsGetU16(nvs_handle_t h, const char* prefix, int idx, uint16_t* v) {
    char k[16]; snprintf(k, sizeof(k), "%s_%d", prefix, idx);
    return nvs_get_u16(h, k, v);
}

esp_err_t nvsGetU8(nvs_handle_t h, const char* prefix, int idx, uint8_t* v) {
    char k[16]; snprintf(k, sizeof(k), "%s_%d", prefix, idx);
    return nvs_get_u8(h, k, v);
}

} // anonymous

bool persistServersToNvs(const ServerRuntime* servers, int count) {
    nvs_handle_t h;
    esp_err_t e = nvs_open(NVS_NS_SERVERS, NVS_READWRITE, &h);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open servers rw: %d", (int)e);
        return false;
    }

    // Amendment E: tail-erase before re-write so stale keys from
    // a higher previous count don't linger.
    nvs_erase_all(h);

    nvs_set_u8(h, "count", static_cast<uint8_t>(count));
    for (int i = 0; i < count; ++i) {
        const ServerCreds& c = servers[i].creds;
        nvsSetStr(h, "n",  i, c.name);
        nvsSetStr(h, "h",  i, c.host);
        nvsSetU16(h, "p",  i, c.port);
        nvsSetStr(h, "u",  i, c.username);
        nvsSetStr(h, "pw", i, c.password);
        nvsSetU8 (h, "ka", i, c.use_key_auth ? 1 : 0);
        nvsSetStr(h, "kp", i, c.key_path);
    }
    e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit: %d", (int)e);
        return false;
    }
    return true;
}

int loadServersFromNvs(ServerRuntime* out, int max) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_SERVERS, NVS_READONLY, &h) != ESP_OK) return 0;

    uint8_t count = 0;
    nvs_get_u8(h, "count", &count);
    int n = count;
    if (n > max) n = max;

    for (int i = 0; i < n; ++i) {
        ServerCreds& c = out[i].creds;
        nvsGetStr(h, "n",  i, c.name,     sizeof(c.name));
        nvsGetStr(h, "h",  i, c.host,     sizeof(c.host));
        nvsGetU16(h, "p",  i, &c.port);
        nvsGetStr(h, "u",  i, c.username, sizeof(c.username));
        nvsGetStr(h, "pw", i, c.password, sizeof(c.password));
        uint8_t ka = 0;
        nvsGetU8 (h, "ka", i, &ka);
        c.use_key_auth = (ka != 0);
        nvsGetStr(h, "kp", i, c.key_path, sizeof(c.key_path));
        out[i].valid = (c.name[0] != '\0' && c.host[0] != '\0');
    }
    nvs_close(h);
    return n;
}

void persistActiveIndex(int index) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_SETTINGS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "active_srv", static_cast<uint8_t>(index));
    nvs_commit(h);
    nvs_close(h);
}

int loadActiveIndex(int max_valid) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_SETTINGS, NVS_READONLY, &h) != ESP_OK) return 0;
    uint8_t idx = 0;
    nvs_get_u8(h, "active_srv", &idx);
    nvs_close(h);
    if (max_valid > 0 && idx >= max_valid) return 0;
    return idx;
}

} // namespace sdcard
