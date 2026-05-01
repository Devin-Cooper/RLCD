#include "settings.hpp"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <cstring>

static const char* TAG = "settings";

namespace app {

static constexpr const char* NVS_NAMESPACE = "app_settings";

static uint16_t clampCardDwellMs(uint16_t v) {
    if (v < 1000) return 1000;
    if (v > 15000) return 15000;
    return v;
}

Settings defaultSettings() {
    Settings s{};
    s.font_size = 1;            // 6x9 — good balance of density and readability
    s.scrollback_depth = 500;
    std::memset(s.ssh_host, 0, sizeof(s.ssh_host));
    s.ssh_port = 22;
    std::memset(s.ssh_user, 0, sizeof(s.ssh_user));
    s.dashboard_interval_ms = 5000;
    s.dashboard_card_dwell_ms = 3000;
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

    nvs_get_u16(handle, "dash_interval", &s.dashboard_interval_ms);

    nvs_get_u16(handle, "card_dwell", &s.dashboard_card_dwell_ms);

    nvs_close(handle);

    // Clamp font_size to valid range
    if (s.font_size > 2) {
        s.font_size = 1;
    }

    s.dashboard_card_dwell_ms = clampCardDwellMs(s.dashboard_card_dwell_ms);

    ESP_LOGI(TAG, "Loaded settings: font=%d scrollback=%d interval=%dms card_dwell=%dms",
             s.font_size, s.scrollback_depth, s.dashboard_interval_ms,
             s.dashboard_card_dwell_ms);

    return s;
}

bool saveSettings(const Settings& s) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open for write failed: %s", esp_err_to_name(err));
        return false;
    }
    nvs_set_u8 (handle, "font_size",     s.font_size);
    nvs_set_u16(handle, "scrollback",    s.scrollback_depth);
    nvs_set_u16(handle, "dash_interval", s.dashboard_interval_ms);
    nvs_set_u16(handle, "card_dwell",    clampCardDwellMs(s.dashboard_card_dwell_ms));
    // Legacy ssh_* fields are no longer written (migrated to new servers NVS namespace).

    err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "Settings saved");
    return true;
}

} // namespace app
