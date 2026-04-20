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

MigrationResult runLegacyMigration(ServerRuntime* servers,
                                    int* count,
                                    int max_servers) {
    if (!servers || !count) return MigrationResult::None;
    if (*count > max_servers) *count = max_servers;

    // Probe legacy ssh_creds (srv_p_0 is the canary)
    bool have_ssh_creds = false;
    {
        nvs_handle_t h;
        if (nvs_open("ssh_creds", NVS_READONLY, &h) == ESP_OK) {
            char pw[64]; size_t len = sizeof(pw);
            if (nvs_get_str(h, "srv_p_0", pw, &len) == ESP_OK) have_ssh_creds = true;
            nvs_close(h);
        }
    }

    // Probe legacy app_settings.ssh_host
    char legacy_host[64] = {};
    char legacy_user[32] = {};
    uint16_t legacy_port = 22;
    {
        nvs_handle_t h;
        if (nvs_open("app_settings", NVS_READONLY, &h) == ESP_OK) {
            size_t len = sizeof(legacy_host);
            nvs_get_str(h, "ssh_host", legacy_host, &len);
            len = sizeof(legacy_user);
            nvs_get_str(h, "ssh_user", legacy_user, &len);
            nvs_get_u16(h, "ssh_port", &legacy_port);
            nvs_close(h);
        }
    }
    bool have_legacy_host = (legacy_host[0] != '\0');

    // PathA / BeltAndSuspenders: ssh_creds + at least one SD-loaded identity
    if (have_ssh_creds && *count > 0) {
        nvs_handle_t h;
        if (nvs_open("ssh_creds", NVS_READONLY, &h) == ESP_OK) {
            for (int i = 0; i < *count; ++i) {
                char pw[64] = {}; size_t len = sizeof(pw);
                char k[16]; snprintf(k, sizeof(k), "srv_p_%d", i);
                if (nvs_get_str(h, k, pw, &len) == ESP_OK) {
                    std::strncpy(servers[i].creds.password, pw,
                                 sizeof(servers[i].creds.password) - 1);
                }
            }
            nvs_close(h);
        }
        persistServersToNvs(servers, *count);

        // Erase legacy ssh_creds
        if (nvs_open("ssh_creds", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_all(h); nvs_commit(h); nvs_close(h);
        }

        if (have_legacy_host) {
            // Belt-and-suspenders: also clear legacy ssh_host fields
            if (nvs_open("app_settings", NVS_READWRITE, &h) == ESP_OK) {
                nvs_erase_key(h, "ssh_host");
                nvs_erase_key(h, "ssh_port");
                nvs_erase_key(h, "ssh_user");
                nvs_erase_key(h, "auth_method");
                nvs_commit(h); nvs_close(h);
            }
            return MigrationResult::BeltAndSuspenders;
        }
        return MigrationResult::PathA;
    }

    // PathAHole: ssh_creds present but no SD identities to pair with.
    // Leave intact for a future boot where SD provides names.
    if (have_ssh_creds && *count == 0) {
        return MigrationResult::PathAHole;
    }

    // PathB: single-server from legacy app_settings.ssh_host
    if (have_legacy_host) {
        if (*count >= max_servers) return MigrationResult::None;
        ServerCreds c{};
        std::strncpy(c.name, "default", sizeof(c.name) - 1);
        std::strncpy(c.host, legacy_host, sizeof(c.host) - 1);
        c.port = legacy_port;
        std::strncpy(c.username, legacy_user, sizeof(c.username) - 1);
        servers[*count].creds = c;
        servers[*count].valid = true;
        (*count)++;
        persistServersToNvs(servers, *count);
        return MigrationResult::PathB;
    }

    return MigrationResult::None;
}

} // namespace sdcard
