#pragma once

#include <cstdint>
#include "wifi_manager.hpp"

namespace sdcard {

static constexpr int MAX_SERVERS = 8;
static constexpr int MAX_DASHBOARD_COMMANDS = 10;

/// Settings parsed from SD card config.json (applied by caller via app::Settings)
struct ParsedSettings {
    uint8_t font_size;
    uint16_t scrollback;
    uint16_t dashboard_interval_ms;
    bool has_font_size;
    bool has_scrollback;
    bool has_dashboard_interval_ms;
};

struct DashboardCommand {
    char label[16];
    char command[128];
};

struct ServerConfig {
    char name[32];
    char host[64];
    uint16_t port;
    char username[32];
    char key_path[64];      // LittleFS path after import
    char key_file_name[64]; // Source key filename from SD card JSON
    bool use_key_auth;
    DashboardCommand dashboard[MAX_DASHBOARD_COMMANDS];
    int dashboard_count;
    bool valid;             // Set to true if parsed successfully
};

/// Manages multi-server configuration loaded from SD card JSON files.
/// Imports WiFi credentials to NVS, SSH keys to LittleFS, and scrubs
/// secrets from the SD card after import.
class ConfigManager {
public:
    ConfigManager();

    /// Full init sequence: load global config, load servers, import keys, scrub.
    /// Call after SD card is mounted and NVS/LittleFS are initialized.
    /// Returns number of servers loaded.
    int init(wifi::WifiManager& wifi_mgr);

    /// Parse /sdcard/config.json — import WiFi via WifiManager, apply settings.
    bool loadGlobalConfig(wifi::WifiManager& wifi_mgr);

    /// Scan /sdcard/servers/*.json — parse each into servers_ array.
    int loadServerConfigs();

    /// Copy SSH key files from SD to LittleFS.
    void importKeys();

    /// Blank passwords and delete key files from SD card.
    void scrubSecrets();

    int serverCount() const { return server_count_; }
    const ServerConfig& getServer(int index) const;
    int activeServerIndex() const { return active_index_; }
    void setActiveServer(int index);
    const ServerConfig& activeServer() const;

    /// Get parsed device settings from config.json (caller applies to app::Settings)
    const ParsedSettings& parsedSettings() const { return parsed_settings_; }

private:
    ServerConfig servers_[MAX_SERVERS];
    int server_count_;
    int active_index_;
    ParsedSettings parsed_settings_;

    bool parseServerJson(const char* path, ServerConfig& out, int index);
    bool copyFile(const char* src, const char* dst);
    void scrubJsonFile(const char* path);
    void scrubGlobalConfig();
};

} // namespace sdcard
