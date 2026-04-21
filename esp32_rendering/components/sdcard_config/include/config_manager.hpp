#pragma once

#include <cstdint>
#include <vector>
#include "wifi_manager.hpp"

namespace sdcard {

static constexpr int MAX_SERVERS = 8;
static constexpr int MAX_DASHBOARD_COMMANDS = 10;

enum class MigrationResult : uint8_t {
    None,
    PathA,               // ssh_creds + SD server identities paired
    PathAHole,           // ssh_creds present but no SD identities — preserved for next boot
    PathB,               // Only legacy ssh_host — seed single "default" server
    BeltAndSuspenders,   // Both ssh_creds+SD AND legacy ssh_host — migrate SD, clear ssh_host
};

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

struct ServerCreds {
    char name[32];
    char host[64];
    uint16_t port;
    char username[32];
    char password[64];      // NVS-destined (not written by parseServerJson yet; Task 19)
    bool use_key_auth;
    char ssh_key_id[33];    // 32 hex chars + null; empty when !use_key_auth
};

struct ServerRuntime {
    ServerCreds creds;
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

    /// Blank passwords and delete key files from SD card.
    void scrubSecrets();

    int serverCount() const { return server_count_; }
    const ServerRuntime& getServer(int index) const;
    int activeServerIndex() const { return active_index_; }
    void setActiveServer(int index);
    const ServerRuntime& activeServer() const;

    // NVS store (delegates to config_store_nvs.cpp for host-testability).
    int loadFromNvs();
    bool persistToNvs();
    int upsertServer(const ServerCreds& creds, int index = -1);
    bool deleteServer(int index);
    // setActiveServer already exists — its body also persists to NVS.

    /// Run one-shot legacy migration (delegates to free function in config_store_nvs).
    MigrationResult migrateLegacyOnce();
    MigrationResult lastMigration() const { return last_migration_; }

    int invalidJsonCount() const { return invalid_json_count_; }

    /// Get parsed device settings from config.json (caller applies to app::Settings)
    const ParsedSettings& parsedSettings() const { return parsed_settings_; }

    /// Indices of servers that loaded with use_key_auth=true but empty
    /// ssh_key_id. Populated after load. Drained by ServerListScreen::onEnter
    /// (one-Toast-per-boot per server); cleared by markRepicked() on picker
    /// success. Runtime-only state; no NVS.
    const std::vector<int>& needsRepickIndices() const { return needs_repick_; }
    void markRepicked(int index);

private:
    ServerRuntime servers_[MAX_SERVERS];
    int server_count_;
    int active_index_;
    ParsedSettings parsed_settings_;
    MigrationResult last_migration_ = MigrationResult::None;
    int invalid_json_count_ = 0;
    std::vector<int> needs_repick_;

    int upsertFromSdDir();
    void persistDashboardTo(const ServerRuntime& s);
    void loadDashboardFor(ServerRuntime& s);
    bool parseServerJson(const char* path, ServerRuntime& out, int index);
    void scrubJsonFile(const char* path);
    void scrubGlobalConfig();
};

} // namespace sdcard
