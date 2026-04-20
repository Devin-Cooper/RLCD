#include "test_console_response.hpp"
#include "test_console_context.hpp"

#include "esp_console.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_manager.hpp"
#include "config_manager.hpp"
#include "config_store_nvs.hpp"
#include "settings.hpp"

#include "cJSON.h"

#include <cstring>
#include <cstdlib>

namespace test_console {

// Base64 decoder used by server-upsert. Amendment B: only the idx-lambda
// path is kept; the dead int8_t T[256] table (designated-range init with
// overlapping ranges) has been dropped.
static int b64_decode(const char* in, uint8_t* out, size_t out_cap) {
    auto idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    size_t n = 0, len = strlen(in);
    for (size_t i = 0; i + 3 < len && in[i] != '='; i += 4) {
        int v0 = idx(in[i]), v1 = idx(in[i+1]);
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

// --- nvs-get <ns> <key> ---
static int cmd_nvs_get(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: nvs-get <ns> <key>"); return 1; }
    nvs_handle_t h;
    if (nvs_open(argv[1], NVS_READONLY, &h) != ESP_OK) { err(2, "namespace not found"); return 1; }
    uint8_t u8; uint16_t u16; uint32_t u32;
    if (nvs_get_u8 (h, argv[2], &u8)  == ESP_OK) { ok("u8 %u",  u8);  nvs_close(h); return 0; }
    if (nvs_get_u16(h, argv[2], &u16) == ESP_OK) { ok("u16 %u", u16); nvs_close(h); return 0; }
    if (nvs_get_u32(h, argv[2], &u32) == ESP_OK) { ok("u32 %lu", (unsigned long)u32); nvs_close(h); return 0; }
    size_t len = 0;
    if (nvs_get_str(h, argv[2], nullptr, &len) == ESP_OK && len <= 256) {
        char buf[256] = {};
        if (nvs_get_str(h, argv[2], buf, &len) == ESP_OK) {
            ok("str %s", buf);
            nvs_close(h);
            return 0;
        }
    }
    err(3, "key not found or type unsupported");
    nvs_close(h);
    return 1;
}

// --- nvs-set <ns> <key> <type> <value...> ---
static int cmd_nvs_set(int argc, char** argv) {
    if (argc < 5) { err(1, "usage: nvs-set <ns> <key> <type> <value>"); return 1; }
    nvs_handle_t h;
    if (nvs_open(argv[1], NVS_READWRITE, &h) != ESP_OK) { err(2, "nvs_open rw failed"); return 1; }

    esp_err_t e = ESP_FAIL;
    if      (!strcmp(argv[3], "u8"))  e = nvs_set_u8 (h, argv[2], (uint8_t) atoi(argv[4]));
    else if (!strcmp(argv[3], "u16")) e = nvs_set_u16(h, argv[2], (uint16_t)atoi(argv[4]));
    else if (!strcmp(argv[3], "u32")) e = nvs_set_u32(h, argv[2], (uint32_t)strtoul(argv[4], nullptr, 0));
    else if (!strcmp(argv[3], "i32")) e = nvs_set_i32(h, argv[2], (int32_t) strtol (argv[4], nullptr, 0));
    else if (!strcmp(argv[3], "str")) {
        char v[256] = {};
        size_t pos = 0;
        for (int i = 4; i < argc && pos + 1 < sizeof(v); ++i) {
            if (i > 4) v[pos++] = ' ';
            size_t rem = sizeof(v) - 1 - pos;
            size_t a = strnlen(argv[i], rem);
            memcpy(v + pos, argv[i], a);
            pos += a;
        }
        v[pos] = '\0';
        e = nvs_set_str(h, argv[2], v);
    }
    else { err(3, "bad type '%s'", argv[3]); nvs_close(h); return 1; }

    if (e != ESP_OK) { err(4, "nvs_set failed: %d", (int)e); nvs_close(h); return 1; }
    nvs_commit(h);
    nvs_close(h);
    ok("%s", "");
    return 0;
}

// --- nvs-rm <ns> <key> ---
static int cmd_nvs_rm(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: nvs-rm <ns> <key>"); return 1; }
    nvs_handle_t h;
    if (nvs_open(argv[1], NVS_READWRITE, &h) != ESP_OK) { err(2, "nvs_open rw failed"); return 1; }
    nvs_erase_key(h, argv[2]);
    nvs_commit(h);
    nvs_close(h);
    ok("%s", "");
    return 0;
}

// --- nvs-erase <ns> ---
static int cmd_nvs_erase(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: nvs-erase <ns>"); return 1; }
    nvs_handle_t h;
    if (nvs_open(argv[1], NVS_READWRITE, &h) != ESP_OK) {
        ok("%s", "");   // already absent
        return 0;
    }
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
    ok("%s", "");
    return 0;
}

// --- wifi-save <ssid> <password> ---
static int cmd_wifi_save(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: wifi-save <ssid> <password>"); return 1; }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    ctx->wifiMgr.saveNetwork(argv[1], argv[2]);
    ok("%s", "");
    return 0;
}

// --- wifi-forget <ssid> ---
static int cmd_wifi_forget(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: wifi-forget <ssid>"); return 1; }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    ctx->wifiMgr.forgetNetwork(argv[1]);
    ok("%s", "");
    return 0;
}

// --- wifi-known ---
static int cmd_wifi_known(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    wifi::NetworkInfo buf[wifi::WifiManager::MAX_KNOWN_NETWORKS];
    int n = ctx->wifiMgr.knownNetworks(buf, wifi::WifiManager::MAX_KNOWN_NETWORKS);
    for (int i = 0; i < n; ++i) data("%s", buf[i].ssid);
    ok("%s", "");
    return 0;
}

// --- server-upsert <json-base64> ---
static int cmd_server_upsert(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: server-upsert <json-base64>"); return 1; }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }

    uint8_t decoded[512];
    int n = b64_decode(argv[1], decoded, sizeof(decoded) - 1);
    if (n < 0) { err(2, "base64 decode"); return 1; }
    decoded[n] = '\0';

    cJSON* root = cJSON_Parse(reinterpret_cast<const char*>(decoded));
    if (!root) { err(3, "json parse"); return 1; }

    sdcard::ServerCreds c{};
    const cJSON* v;
    if ((v = cJSON_GetObjectItem(root, "name"))     && cJSON_IsString(v))
        strncpy(c.name,     v->valuestring, sizeof(c.name)     - 1);
    if ((v = cJSON_GetObjectItem(root, "host"))     && cJSON_IsString(v))
        strncpy(c.host,     v->valuestring, sizeof(c.host)     - 1);
    if ((v = cJSON_GetObjectItem(root, "port"))     && cJSON_IsNumber(v))
        c.port = (uint16_t)v->valueint;
    if ((v = cJSON_GetObjectItem(root, "username")) && cJSON_IsString(v))
        strncpy(c.username, v->valuestring, sizeof(c.username) - 1);
    if ((v = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(v))
        strncpy(c.password, v->valuestring, sizeof(c.password) - 1);
    cJSON_Delete(root);

    int idx = -1;
    for (int i = 0; i < ctx->configMgr.serverCount(); ++i) {
        if (strcmp(ctx->configMgr.getServer(i).creds.name, c.name) == 0) {
            idx = i; break;
        }
    }
    int written = ctx->configMgr.upsertServer(c, idx);
    if (written < 0) { err(4, "upsert failed / limit"); return 1; }
    ok("%d", written);
    return 0;
}

// --- server-delete <index> ---
static int cmd_server_delete(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: server-delete <index>"); return 1; }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    int idx = atoi(argv[1]);
    if (!ctx->configMgr.deleteServer(idx)) { err(2, "delete failed"); return 1; }
    ok("%s", "");
    return 0;
}

// --- server-set-active <index> ---
static int cmd_server_set_active(int argc, char** argv) {
    if (argc != 2) { err(1, "usage: server-set-active <index>"); return 1; }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    int idx = atoi(argv[1]);
    ctx->configMgr.setActiveServer(idx);
    ok("%s", "");
    return 0;
}

// --- server-list ---
static int cmd_server_list(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    int active = ctx->configMgr.activeServerIndex();
    for (int i = 0; i < ctx->configMgr.serverCount(); ++i) {
        const auto& c = ctx->configMgr.getServer(i).creds;
        data("%d %s %s@%s:%d %s",
             i, c.name, c.username, c.host, c.port,
             i == active ? "active" : "inactive");
    }
    ok("%s", "");
    return 0;
}

// --- settings-set <field> <value> ---
static int cmd_settings_set(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: settings-set <field> <value>"); return 1; }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }

    if      (!strcmp(argv[1], "font_size"))              ctx->settings.font_size              = (uint8_t) atoi(argv[2]);
    else if (!strcmp(argv[1], "scrollback_depth"))       ctx->settings.scrollback_depth       = (uint16_t)atoi(argv[2]);
    else if (!strcmp(argv[1], "dashboard_interval_ms"))  ctx->settings.dashboard_interval_ms  = (uint16_t)atoi(argv[2]);
    else { err(2, "unknown field"); return 1; }

    if (!app::saveSettings(ctx->settings)) { err(3, "saveSettings failed"); return 1; }
    ok("%s", "");
    return 0;
}

void registerNvsCommands() {
    const esp_console_cmd_t cmds[] = {
        {"nvs-get",           "nvs-get <ns> <key>",                 nullptr, cmd_nvs_get,           nullptr, nullptr, nullptr},
        {"nvs-set",           "nvs-set <ns> <key> <type> <value>",  nullptr, cmd_nvs_set,           nullptr, nullptr, nullptr},
        {"nvs-rm",            "nvs-rm <ns> <key>",                  nullptr, cmd_nvs_rm,            nullptr, nullptr, nullptr},
        {"nvs-erase",         "nvs-erase <ns>",                     nullptr, cmd_nvs_erase,         nullptr, nullptr, nullptr},
        {"wifi-save",         "wifi-save <ssid> <password>",        nullptr, cmd_wifi_save,         nullptr, nullptr, nullptr},
        {"wifi-forget",       "wifi-forget <ssid>",                 nullptr, cmd_wifi_forget,       nullptr, nullptr, nullptr},
        {"wifi-known",        "List known SSIDs",                   nullptr, cmd_wifi_known,        nullptr, nullptr, nullptr},
        {"server-upsert",     "server-upsert <json-base64>",        nullptr, cmd_server_upsert,     nullptr, nullptr, nullptr},
        {"server-delete",     "server-delete <index>",              nullptr, cmd_server_delete,     nullptr, nullptr, nullptr},
        {"server-set-active", "server-set-active <index>",          nullptr, cmd_server_set_active, nullptr, nullptr, nullptr},
        {"server-list",       "List all servers",                   nullptr, cmd_server_list,       nullptr, nullptr, nullptr},
        {"settings-set",      "settings-set <field> <value>",       nullptr, cmd_settings_set,      nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
