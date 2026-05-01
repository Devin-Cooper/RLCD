#pragma once

#include <cstdint>

namespace app {

/// Persistent application settings stored in NVS.
struct Settings {
    uint8_t font_size;              // 0=5x7, 1=6x9, 2=8x12
    uint16_t scrollback_depth;      // default 500
    uint16_t dashboard_interval_ms; // default 5000
    uint16_t dashboard_card_dwell_ms; // default 3000, clamp 1000–15000
    // DEPRECATED (migration-only): these four fields are no longer read or
    // written by load/saveSettings. They remain in the struct to keep
    // migration code compiling; delete in a future release after
    // the migration window closes.
    char ssh_host[64];
    uint16_t ssh_port;              // default 22
    char ssh_user[32];
    uint8_t auth_method;            // 0=password, 1=key
};

/// Load settings from NVS. Returns defaults if NVS read fails.
Settings loadSettings();

/// Save settings to NVS. Returns true on success; false on NVS error.
bool saveSettings(const Settings& s);

/// Return compile-time default settings.
Settings defaultSettings();

} // namespace app
