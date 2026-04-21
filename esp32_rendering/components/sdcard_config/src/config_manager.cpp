#include "config_manager.hpp"
#include "config_store_nvs.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "wifi_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

static const char* TAG = "config_mgr";
static const char* SD_CONFIG_PATH = "/sdcard/config.json";
static const char* SD_SERVERS_DIR = "/sdcard/servers";

namespace sdcard {

static ServerRuntime s_default_server = {};

ConfigManager::ConfigManager()
    : server_count_(0), active_index_(0), parsed_settings_{} {
    std::memset(servers_, 0, sizeof(servers_));
}

int ConfigManager::init(wifi::WifiManager& wifi_mgr) {
    // 1. Load from NVS first (post-migration steady state)
    loadFromNvs();

    // 2. Load global SD config (WiFi imports, device settings parse)
    loadGlobalConfig(wifi_mgr);

    // 3. SD servers upsert by name
    int sd_added = upsertFromSdDir();
    if (sd_added > 0) {
        scrubSecrets();
    }

    // 4. Migration (runs if legacy present)
    migrateLegacyOnce();

    // 5. Per-server dashboard commands from LittleFS
    for (int i = 0; i < server_count_; ++i) {
        loadDashboardFor(servers_[i]);
    }

    return server_count_;
}

// --- Global Config ---

bool ConfigManager::loadGlobalConfig(wifi::WifiManager& wifi_mgr) {
    ESP_LOGI(TAG, "Opening %s", SD_CONFIG_PATH);
    FILE* f = fopen(SD_CONFIG_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "No %s found", SD_CONFIG_PATH);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    ESP_LOGI(TAG, "config.json size: %ld bytes", size);

    if (size <= 0 || size > 8192) {
        ESP_LOGW(TAG, "config.json invalid size: %ld", size);
        fclose(f);
        return false;
    }

    char* buf = static_cast<char*>(malloc(size + 1));
    if (!buf) {
        ESP_LOGE(TAG, "OOM reading config.json (%ld bytes)", size);
        fclose(f);
        return false;
    }
    size_t bytes_read = fread(buf, 1, size, f);
    buf[bytes_read] = '\0';
    fclose(f);
    ESP_LOGI(TAG, "Read %d bytes from config.json", (int)bytes_read);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "config.json JSON parse failed");
        return false;
    }
    ESP_LOGI(TAG, "config.json parsed OK");

    // Import WiFi networks via WifiManager
    cJSON* wifi = cJSON_GetObjectItem(root, "wifi");
    if (cJSON_IsArray(wifi)) {
        int n = cJSON_GetArraySize(wifi);
        ESP_LOGI(TAG, "Found %d WiFi network(s) in config", n);
        for (int i = 0; i < n; i++) {
            cJSON* net = cJSON_GetArrayItem(wifi, i);
            cJSON* ssid = cJSON_GetObjectItem(net, "ssid");
            cJSON* pass = cJSON_GetObjectItem(net, "password");
            if (cJSON_IsString(ssid) && cJSON_IsString(pass) &&
                strlen(pass->valuestring) > 0) {
                ESP_LOGI(TAG, "Saving WiFi[%d]: SSID='%s' pass_len=%d",
                         i, ssid->valuestring, (int)strlen(pass->valuestring));
                wifi_mgr.saveNetwork(ssid->valuestring, pass->valuestring);
                ESP_LOGI(TAG, "saveNetwork returned for '%s'", ssid->valuestring);
            } else {
                ESP_LOGW(TAG, "WiFi[%d]: missing ssid/pass or empty password", i);
            }
        }
    } else {
        ESP_LOGW(TAG, "No 'wifi' array in config.json");
    }

    // Parse device settings (caller applies via app::Settings)
    cJSON* settings = cJSON_GetObjectItem(root, "settings");
    if (cJSON_IsObject(settings)) {
        cJSON* v;
        if ((v = cJSON_GetObjectItem(settings, "font_size")) && cJSON_IsNumber(v)) {
            parsed_settings_.font_size = static_cast<uint8_t>(v->valueint);
            parsed_settings_.has_font_size = true;
        }
        if ((v = cJSON_GetObjectItem(settings, "scrollback")) && cJSON_IsNumber(v)) {
            parsed_settings_.scrollback = static_cast<uint16_t>(v->valueint);
            parsed_settings_.has_scrollback = true;
        }
        if ((v = cJSON_GetObjectItem(settings, "dashboard_interval_ms")) && cJSON_IsNumber(v)) {
            parsed_settings_.dashboard_interval_ms = static_cast<uint16_t>(v->valueint);
            parsed_settings_.has_dashboard_interval_ms = true;
        }
        ESP_LOGI(TAG, "Parsed device settings from SD card");
    }

    cJSON_Delete(root);
    return true;
}

// --- Server Configs ---

int ConfigManager::upsertFromSdDir() {
    DIR* dir = opendir(SD_SERVERS_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "No %s directory", SD_SERVERS_DIR);
        return 0;
    }
    int added = 0;
    int overflow = 0;
    invalid_json_count_ = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        size_t len = strlen(name);
        if (len < 6 || strcmp(name + len - 5, ".json") != 0) continue;

        char path[288];
        snprintf(path, sizeof(path), "%s/%s", SD_SERVERS_DIR, name);

        ServerRuntime tmp{};
        if (!parseServerJson(path, tmp, 0)) {
            invalid_json_count_++;
            continue;
        }

        // Match by name
        int idx = -1;
        for (int i = 0; i < server_count_; ++i) {
            if (std::strncmp(servers_[i].creds.name, tmp.creds.name, 32) == 0) {
                idx = i; break;
            }
        }
        int written = upsertServer(tmp.creds, idx);
        if (written < 0) { overflow++; continue; }

        // Copy runtime-only fields
        servers_[written].dashboard_count = tmp.dashboard_count;
        std::memcpy(servers_[written].dashboard, tmp.dashboard,
                    sizeof(tmp.dashboard));
        added++;

        persistDashboardTo(servers_[written]);
    }
    closedir(dir);
    if (overflow > 0) {
        ESP_LOGW(TAG, "SD upsert skipped %d server(s) due to MAX_SERVERS limit",
                 overflow);
    }
    ESP_LOGI(TAG, "SD upsert: added/updated %d server(s), %d invalid",
             added, invalid_json_count_);
    return added;
}

// Legacy alias — kept for any call-sites not yet migrated to upsertFromSdDir.
// Remove in Task 25 cleanup.
int ConfigManager::loadServerConfigs() {
    return upsertFromSdDir();
}

bool ConfigManager::parseServerJson(const char* path, ServerRuntime& out, int index) {
    FILE* f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 8192) {
        fclose(f);
        return false;
    }

    char* buf = static_cast<char*>(malloc(size + 1));
    if (!buf) {
        ESP_LOGE(TAG, "OOM parsing server JSON %s (%ld bytes)", path, size);
        fclose(f);
        return false;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse %s", path);
        return false;
    }

    std::memset(&out, 0, sizeof(out));

    cJSON* v;
    if ((v = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(v))
        strncpy(out.creds.name, v->valuestring, sizeof(out.creds.name) - 1);
    if ((v = cJSON_GetObjectItem(root, "host")) && cJSON_IsString(v))
        strncpy(out.creds.host, v->valuestring, sizeof(out.creds.host) - 1);
    if ((v = cJSON_GetObjectItem(root, "port")) && cJSON_IsNumber(v))
        out.creds.port = static_cast<uint16_t>(v->valueint);
    else
        out.creds.port = 22;
    if ((v = cJSON_GetObjectItem(root, "username")) && cJSON_IsString(v))
        strncpy(out.creds.username, v->valuestring, sizeof(out.creds.username) - 1);

    // Auth method
    if ((v = cJSON_GetObjectItem(root, "auth_method")) && cJSON_IsString(v))
        out.creds.use_key_auth = (strcmp(v->valuestring, "key") == 0);

    // Key assignment happens via UI (SshKeyListScreen picker); no ssh_key_id
    // persists through SD-card JSON parsing.
    out.creds.ssh_key_id[0] = '\0';

    // Password goes into ServerCreds directly; upsertServer persists it
    // via the new `servers` NVS namespace.
    (void)index;
    if ((v = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(v) &&
        strlen(v->valuestring) > 0) {
        strncpy(out.creds.password, v->valuestring, sizeof(out.creds.password) - 1);
    }

    // Dashboard commands
    cJSON* dashboard = cJSON_GetObjectItem(root, "dashboard");
    if (cJSON_IsArray(dashboard)) {
        int n = cJSON_GetArraySize(dashboard);
        if (n > MAX_DASHBOARD_COMMANDS) n = MAX_DASHBOARD_COMMANDS;
        for (int i = 0; i < n; i++) {
            cJSON* cmd = cJSON_GetArrayItem(dashboard, i);
            cJSON* label = cJSON_GetObjectItem(cmd, "label");
            cJSON* command = cJSON_GetObjectItem(cmd, "command");
            if (cJSON_IsString(label) && cJSON_IsString(command)) {
                strncpy(out.dashboard[i].label, label->valuestring,
                        sizeof(out.dashboard[i].label) - 1);
                strncpy(out.dashboard[i].command, command->valuestring,
                        sizeof(out.dashboard[i].command) - 1);
                out.dashboard_count++;
            }
        }
    }

    // Require at minimum a name and host
    bool valid = (out.creds.name[0] != '\0' && out.creds.host[0] != '\0');

    cJSON_Delete(root);
    return valid;
}

// --- Legacy Migration ---

MigrationResult ConfigManager::migrateLegacyOnce() {
    MigrationResult r = runLegacyMigration(servers_, &server_count_, MAX_SERVERS);
    last_migration_ = r;
    return r;
}

// --- Per-Server Dashboard (LittleFS .cmd files) ---

void ConfigManager::persistDashboardTo(const ServerRuntime& s) {
    mkdir("/littlefs/servers", 0755);   // idempotent
    char path[96];
    snprintf(path, sizeof(path), "/littlefs/servers/%s.cmd", s.creds.name);
    char tmp[sizeof(path) + 4];   // +4 for ".tmp" — silences -Werror=format-truncation
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE* f = fopen(tmp, "w");
    if (!f) { ESP_LOGW(TAG, "open %s: %d", tmp, errno); return; }
    for (int i = 0; i < s.dashboard_count; ++i) {
        fprintf(f, "%s|%s\n", s.dashboard[i].label, s.dashboard[i].command);
    }
    fclose(f);
    rename(tmp, path);
}

void ConfigManager::loadDashboardFor(ServerRuntime& s) {
    char path[96];
    snprintf(path, sizeof(path), "/littlefs/servers/%s.cmd", s.creds.name);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[256];
    s.dashboard_count = 0;
    while (fgets(line, sizeof(line), f) &&
           s.dashboard_count < MAX_DASHBOARD_COMMANDS) {
        char* bar = std::strchr(line, '|');
        if (!bar) continue;
        *bar = '\0';
        char* cmd = bar + 1;
        char* nl = std::strchr(cmd, '\n'); if (nl) *nl = '\0';
        std::strncpy(s.dashboard[s.dashboard_count].label, line,
                     sizeof(s.dashboard[0].label) - 1);
        std::strncpy(s.dashboard[s.dashboard_count].command, cmd,
                     sizeof(s.dashboard[0].command) - 1);
        s.dashboard_count++;
    }
    fclose(f);
}

// --- Secret Scrubbing ---

void ConfigManager::scrubSecrets() {
    // Scrub server JSON files
    DIR* dir = opendir(SD_SERVERS_DIR);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            const char* name = entry->d_name;
            size_t len = strlen(name);

            if (len >= 6 && strcmp(name + len - 5, ".json") == 0) {
                // Scrub JSON file
                char path[288];
                snprintf(path, sizeof(path), "%s/%s", SD_SERVERS_DIR, name);
                scrubJsonFile(path);
            } else if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                // Delete non-JSON files (key files)
                char path[288];
                snprintf(path, sizeof(path), "%s/%s", SD_SERVERS_DIR, name);
                if (remove(path) == 0) {
                    ESP_LOGI(TAG, "Deleted key file: %s", path);
                }
            }
        }
        closedir(dir);
    }

    // Scrub global config
    scrubGlobalConfig();
}

void ConfigManager::scrubJsonFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = static_cast<char*>(malloc(size + 1));
    if (!buf) {
        ESP_LOGE(TAG, "OOM scrubbing %s (%ld bytes)", path, size);
        fclose(f);
        return;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) return;

    bool modified = false;

    // Blank password if non-empty
    cJSON* pass = cJSON_GetObjectItem(root, "password");
    if (cJSON_IsString(pass) && strlen(pass->valuestring) > 0) {
        cJSON_SetValuestring(pass, "");
        modified = true;
    }

    // Remove key_file field
    if (cJSON_HasObjectItem(root, "key_file")) {
        cJSON_DeleteItemFromObject(root, "key_file");
        modified = true;
    }

    if (modified) {
        char* out = cJSON_PrintUnformatted(root);
        if (out) {
            f = fopen(path, "w");
            if (f) {
                fputs(out, f);
                fclose(f);
                ESP_LOGI(TAG, "Scrubbed secrets from %s", path);
            }
            cJSON_free(out);
        }
    }

    cJSON_Delete(root);
}

void ConfigManager::scrubGlobalConfig() {
    FILE* f = fopen(SD_CONFIG_PATH, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = static_cast<char*>(malloc(size + 1));
    if (!buf) {
        ESP_LOGE(TAG, "OOM scrubbing %s (%ld bytes)", SD_CONFIG_PATH, size);
        fclose(f);
        return;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) return;

    bool modified = false;
    cJSON* wifi = cJSON_GetObjectItem(root, "wifi");
    if (cJSON_IsArray(wifi)) {
        int n = cJSON_GetArraySize(wifi);
        for (int i = 0; i < n; i++) {
            cJSON* net = cJSON_GetArrayItem(wifi, i);
            cJSON* pass = cJSON_GetObjectItem(net, "password");
            if (cJSON_IsString(pass) && strlen(pass->valuestring) > 0) {
                cJSON_SetValuestring(pass, "");
                modified = true;
            }
        }
    }

    if (modified) {
        char* out = cJSON_PrintUnformatted(root);
        if (out) {
            f = fopen(SD_CONFIG_PATH, "w");
            if (f) {
                fputs(out, f);
                fclose(f);
                ESP_LOGI(TAG, "Scrubbed WiFi passwords from config.json");
            }
            cJSON_free(out);
        }
    }

    cJSON_Delete(root);
}

// --- Server Access ---

const ServerRuntime& ConfigManager::getServer(int index) const {
    if (index < 0 || index >= server_count_) return s_default_server;
    return servers_[index];
}

void ConfigManager::setActiveServer(int index) {
    if (index >= 0 && index < server_count_) {
        active_index_ = index;
        persistActiveIndex(index);
        ESP_LOGI(TAG, "Active server: %s", servers_[active_index_].creds.name);
    }
}

int ConfigManager::loadFromNvs() {
    needs_repick_.clear();
    server_count_ = loadServersFromNvs(servers_, MAX_SERVERS);
    active_index_ = loadActiveIndex(server_count_);
    // Track servers that claim key-auth but have no ssh_key_id — UI layer
    // surfaces a Toast on ServerListScreen entry so the user re-picks.
    for (int i = 0; i < server_count_; ++i) {
        if (servers_[i].creds.use_key_auth &&
            servers_[i].creds.ssh_key_id[0] == '\0') {
            needs_repick_.push_back(i);
        }
    }
    return server_count_;
}

void ConfigManager::markRepicked(int index) {
    auto it = std::find(needs_repick_.begin(), needs_repick_.end(), index);
    if (it != needs_repick_.end()) needs_repick_.erase(it);
}

bool ConfigManager::persistToNvs() {
    return persistServersToNvs(servers_, server_count_);
}

int ConfigManager::upsertServer(const ServerCreds& creds, int index) {
    int new_count = server_count_;
    if (index < 0) {
        if (server_count_ >= MAX_SERVERS) return -1;
        index = server_count_;
        new_count = server_count_ + 1;
    } else if (index >= server_count_) {
        return -1;
    }
    // Stage the write, persist, then commit in-memory state only on success.
    ServerRuntime saved = servers_[index];
    servers_[index].creds = creds;
    servers_[index].valid = true;
    int saved_count = server_count_;
    server_count_ = new_count;
    if (!persistToNvs()) {
        // Roll back.
        servers_[index] = saved;
        server_count_ = saved_count;
        return -1;
    }
    return index;
}

bool ConfigManager::deleteServer(int index) {
    if (index < 0 || index >= server_count_) return false;
    for (int i = index; i < server_count_ - 1; ++i) {
        servers_[i] = servers_[i + 1];
    }
    server_count_--;
    if (active_index_ == index) active_index_ = 0;
    else if (active_index_ > index) active_index_--;
    persistActiveIndex(active_index_);
    return persistToNvs();
}

const ServerRuntime& ConfigManager::activeServer() const {
    if (server_count_ == 0) return s_default_server;
    return servers_[active_index_];
}

} // namespace sdcard
