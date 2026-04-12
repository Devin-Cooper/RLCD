#include "config_manager.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "wifi_manager.hpp"

#include <cctype>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

static const char* TAG = "config_mgr";
static const char* SD_CONFIG_PATH = "/sdcard/config.json";
static const char* SD_SERVERS_DIR = "/sdcard/servers";
static const char* KEYS_DIR = "/littlefs/keys";

namespace sdcard {

static ServerConfig s_default_server = {};

ConfigManager::ConfigManager()
    : server_count_(0), active_index_(0), parsed_settings_{} {
    std::memset(servers_, 0, sizeof(servers_));
}

int ConfigManager::init(wifi::WifiManager& wifi_mgr) {
    loadGlobalConfig(wifi_mgr);
    int count = loadServerConfigs();
    if (count > 0) {
        importKeys();
        scrubSecrets();
    }
    return count;
}

// --- Global Config ---

bool ConfigManager::loadGlobalConfig(wifi::WifiManager& wifi_mgr) {
    FILE* f = fopen(SD_CONFIG_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "No %s found", SD_CONFIG_PATH);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 8192) {
        ESP_LOGW(TAG, "config.json invalid size: %ld", size);
        fclose(f);
        return false;
    }

    char* buf = static_cast<char*>(malloc(size + 1));
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "config.json parse failed");
        return false;
    }

    // Import WiFi networks via WifiManager
    cJSON* wifi = cJSON_GetObjectItem(root, "wifi");
    if (cJSON_IsArray(wifi)) {
        int n = cJSON_GetArraySize(wifi);
        for (int i = 0; i < n; i++) {
            cJSON* net = cJSON_GetArrayItem(wifi, i);
            cJSON* ssid = cJSON_GetObjectItem(net, "ssid");
            cJSON* pass = cJSON_GetObjectItem(net, "password");
            if (cJSON_IsString(ssid) && cJSON_IsString(pass) &&
                strlen(pass->valuestring) > 0) {
                wifi_mgr.saveNetwork(ssid->valuestring, pass->valuestring);
                ESP_LOGI(TAG, "Imported WiFi: %s", ssid->valuestring);
            }
        }
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

int ConfigManager::loadServerConfigs() {
    DIR* dir = opendir(SD_SERVERS_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "No %s directory", SD_SERVERS_DIR);
        return 0;
    }

    server_count_ = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr && server_count_ < MAX_SERVERS) {
        // Only process .json files
        const char* name = entry->d_name;
        size_t len = strlen(name);
        if (len < 6 || strcmp(name + len - 5, ".json") != 0) continue;

        char path[288];
        snprintf(path, sizeof(path), "%s/%s", SD_SERVERS_DIR, name);

        if (parseServerJson(path, servers_[server_count_], server_count_)) {
            servers_[server_count_].valid = true;
            ESP_LOGI(TAG, "Loaded server: %s (%s:%d)",
                     servers_[server_count_].name,
                     servers_[server_count_].host,
                     servers_[server_count_].port);
            server_count_++;
        }
    }
    closedir(dir);

    ESP_LOGI(TAG, "Loaded %d server config(s)", server_count_);
    return server_count_;
}

bool ConfigManager::parseServerJson(const char* path, ServerConfig& out, int index) {
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
        strncpy(out.name, v->valuestring, sizeof(out.name) - 1);
    if ((v = cJSON_GetObjectItem(root, "host")) && cJSON_IsString(v))
        strncpy(out.host, v->valuestring, sizeof(out.host) - 1);
    if ((v = cJSON_GetObjectItem(root, "port")) && cJSON_IsNumber(v))
        out.port = static_cast<uint16_t>(v->valueint);
    else
        out.port = 22;
    if ((v = cJSON_GetObjectItem(root, "username")) && cJSON_IsString(v))
        strncpy(out.username, v->valuestring, sizeof(out.username) - 1);

    // Auth method
    if ((v = cJSON_GetObjectItem(root, "auth_method")) && cJSON_IsString(v))
        out.use_key_auth = (strcmp(v->valuestring, "key") == 0);

    // Key file — store the filename for importKeys() to process
    if ((v = cJSON_GetObjectItem(root, "key_file")) && cJSON_IsString(v) &&
        strlen(v->valuestring) > 0) {
        strncpy(out.key_file_name, v->valuestring, sizeof(out.key_file_name) - 1);
        // Build sanitized LittleFS target path (replace non-alnum with '_')
        char safe_name[32];
        strncpy(safe_name, out.name, sizeof(safe_name) - 1);
        safe_name[sizeof(safe_name) - 1] = '\0';
        for (int j = 0; safe_name[j]; j++) {
            if (!isalnum(static_cast<unsigned char>(safe_name[j])))
                safe_name[j] = '_';
        }
        snprintf(out.key_path, sizeof(out.key_path), "/littlefs/keys/%s", safe_name);
        out.use_key_auth = true;
    }

    // Password — save to NVS under index-based key; will be scrubbed from SD
    if ((v = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(v) &&
        strlen(v->valuestring) > 0) {
        nvs_handle_t handle;
        if (nvs_open("ssh_creds", NVS_READWRITE, &handle) == ESP_OK) {
            char nvs_key[16];
            snprintf(nvs_key, sizeof(nvs_key), "srv_p_%d", index);
            nvs_set_str(handle, nvs_key, v->valuestring);
            nvs_commit(handle);
            nvs_close(handle);
        }
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
    bool valid = (out.name[0] != '\0' && out.host[0] != '\0');

    cJSON_Delete(root);
    return valid;
}

// --- Key Import ---

void ConfigManager::importKeys() {
    // Ensure keys directory exists
    mkdir(KEYS_DIR, 0755);

    for (int i = 0; i < server_count_; i++) {
        if (!servers_[i].use_key_auth || servers_[i].key_file_name[0] == '\0') continue;

        // Build source path from stored key_file_name
        char src_path[288];
        snprintf(src_path, sizeof(src_path), "%s/%s",
                 SD_SERVERS_DIR, servers_[i].key_file_name);

        if (copyFile(src_path, servers_[i].key_path)) {
            ESP_LOGI(TAG, "Imported key for %s -> %s",
                     servers_[i].name, servers_[i].key_path);
        } else {
            ESP_LOGW(TAG, "Failed to import key for %s from %s",
                     servers_[i].name, src_path);
            // Fall back to password auth if key import fails
            servers_[i].use_key_auth = false;
            servers_[i].key_path[0] = '\0';
        }
    }
}

bool ConfigManager::copyFile(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }

    fclose(in);
    fclose(out);
    return true;
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

const ServerConfig& ConfigManager::getServer(int index) const {
    if (index < 0 || index >= server_count_) return s_default_server;
    return servers_[index];
}

void ConfigManager::setActiveServer(int index) {
    if (index >= 0 && index < server_count_) {
        active_index_ = index;
        ESP_LOGI(TAG, "Active server: %s", servers_[active_index_].name);
    }
}

const ServerConfig& ConfigManager::activeServer() const {
    if (server_count_ == 0) return s_default_server;
    return servers_[active_index_];
}

} // namespace sdcard
