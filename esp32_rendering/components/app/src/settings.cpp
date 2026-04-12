#include "settings.hpp"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <cstring>

static const char* TAG = "settings";

namespace app {

static constexpr const char* NVS_NAMESPACE = "app_settings";

Settings defaultSettings() {
    Settings s{};
    s.font_size = 1;            // 6x9 — good balance of density and readability
    s.scrollback_depth = 500;
    std::memset(s.ssh_host, 0, sizeof(s.ssh_host));
    s.ssh_port = 22;
    std::memset(s.ssh_user, 0, sizeof(s.ssh_user));
    s.dashboard_interval_ms = 5000;
    s.auth_method = 1;          // key auth preferred (Ed25519, 26ms)
    return s;
}

Settings loadSettings() {
    Settings s = defaultSettings();

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed (%s), using defaults", esp_err_to_name(err));
        return s;
    }

    // Read each field individually — missing keys silently keep default
    nvs_get_u8(handle, "font_size", &s.font_size);

    nvs_get_u16(handle, "scrollback", &s.scrollback_depth);

    size_t len = sizeof(s.ssh_host);
    nvs_get_str(handle, "ssh_host", s.ssh_host, &len);

    nvs_get_u16(handle, "ssh_port", &s.ssh_port);

    len = sizeof(s.ssh_user);
    nvs_get_str(handle, "ssh_user", s.ssh_user, &len);

    nvs_get_u16(handle, "dash_interval", &s.dashboard_interval_ms);

    nvs_get_u8(handle, "auth_method", &s.auth_method);

    nvs_close(handle);

    // Clamp font_size to valid range
    if (s.font_size > 2) {
        s.font_size = 1;
    }

    ESP_LOGI(TAG, "Loaded settings: font=%d host=%s port=%d user=%s interval=%dms auth=%d",
             s.font_size, s.ssh_host, s.ssh_port, s.ssh_user,
             s.dashboard_interval_ms, s.auth_method);

    return s;
}

void saveSettings(const Settings& s) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open for write failed: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_u8(handle, "font_size", s.font_size);
    nvs_set_u16(handle, "scrollback", s.scrollback_depth);
    nvs_set_str(handle, "ssh_host", s.ssh_host);
    nvs_set_u16(handle, "ssh_port", s.ssh_port);
    nvs_set_str(handle, "ssh_user", s.ssh_user);
    nvs_set_u16(handle, "dash_interval", s.dashboard_interval_ms);
    nvs_set_u8(handle, "auth_method", s.auth_method);

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Settings saved");
    }

    nvs_close(handle);
}

} // namespace app
