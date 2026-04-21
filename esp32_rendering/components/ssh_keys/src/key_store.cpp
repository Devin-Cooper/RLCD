#include "ssh_keys.hpp"
#include "ssh_keys_index.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char* TAG = "ssh_keys";
static constexpr const char* NVS_NS = "ssh_keys";
static constexpr const char* NVS_KEY_IDX = "idx";
static constexpr const char* NVS_KEY_WARN = "warn_shown";
static constexpr const char* KEYS_DIR_PATH = "/littlefs/ssh_keys";
static constexpr const char* LEGACY_DIR_PATH = "/littlefs/keys";

namespace ssh_keys {

KeyStore::KeyStore() = default;
KeyStore::~KeyStore() = default;

static void ensure_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) return;
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "mkdir %s: %d", path, errno);
    }
}

bool KeyStore::init() {
    ensure_dir(KEYS_DIR_PATH);

    // Legacy-directory observability (spec §Deletions).
    struct stat st;
    if (stat(LEGACY_DIR_PATH, &st) == 0) {
        ESP_LOGW(TAG, "legacy /littlefs/keys/ dir present — orphan, may be removed manually");
    }

    if (!deserialize_from_nvs()) {
        ESP_LOGW(TAG, "Key index unreadable; starting empty.");
        keys_.clear();
    }
    sweep_orphans();
    // Sort by created_utc desc, name asc
    std::sort(keys_.begin(), keys_.end(), [](const KeyMeta& a, const KeyMeta& b) {
        if (a.created_utc != b.created_utc) return a.created_utc > b.created_utc;
        return std::strcmp(a.name, b.name) < 0;
    });
    ESP_LOGD(TAG, "Store init: %zu keys loaded", keys_.size());
    return true;
}

bool KeyStore::path_for(const char* ssh_key_id, char* out_path, size_t cap) const {
    if (!ssh_key_id || !out_path || cap < 52) return false;  // len("/littlefs/ssh_keys/") + 32 + 1
    auto parsed = KeyId::parse(ssh_key_id);
    if (!parsed) return false;
    if (!contains(*parsed)) return false;
    int n = std::snprintf(out_path, cap, "%s/%s", KEYS_DIR_PATH, ssh_key_id);
    return n > 0 && static_cast<size_t>(n) < cap;
}

bool KeyStore::pub_path_for(const char* ssh_key_id, char* out_path, size_t cap) const {
    if (!ssh_key_id || !out_path || cap < 56) return false;  // len("/littlefs/ssh_keys/") + 32 + ".pub" + 1
    auto parsed = KeyId::parse(ssh_key_id);
    if (!parsed) return false;
    if (!contains(*parsed)) return false;
    int n = std::snprintf(out_path, cap, "%s/%s.pub", KEYS_DIR_PATH, ssh_key_id);
    return n > 0 && static_cast<size_t>(n) < cap;
}

bool KeyStore::contains(const KeyId& id) const {
    return find(id) != nullptr;
}

const KeyMeta* KeyStore::find(const KeyId& id) const {
    for (const auto& m : keys_) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

int KeyStore::max_keys() const {
    return CONFIG_SSH_KEYS_MAX;
}

bool KeyStore::rename(const KeyId& id, const char* new_name) {
    if (!new_name || new_name[0] == '\0') return false;
    size_t nlen = std::strlen(new_name);
    if (nlen >= sizeof(KeyMeta::name)) return false;
    for (const auto& m : keys_) {
        if (m.id == id) continue;
        if (std::strcmp(m.name, new_name) == 0) return false;  // collision
    }
    KeyMeta* target = nullptr;
    for (auto& m : keys_) {
        if (m.id == id) { target = &m; break; }
    }
    if (!target) return false;
    std::memset(target->name, 0, sizeof(target->name));
    std::memcpy(target->name, new_name, nlen);
    if (!serialize_to_nvs()) {
        ESP_LOGE(TAG, "rename: nvs serialize failed");
        return false;
    }
    // Sort may need to change (name asc within same created_utc).
    std::sort(keys_.begin(), keys_.end(), [](const KeyMeta& a, const KeyMeta& b) {
        if (a.created_utc != b.created_utc) return a.created_utc > b.created_utc;
        return std::strcmp(a.name, b.name) < 0;
    });
    return true;
}

bool KeyStore::delete_key(const KeyId& id, ReferenceCheck check, void* user_data) {
    if (check) {
        auto refs = check(id, user_data);
        if (!refs.empty()) {
            ESP_LOGW(TAG, "delete: key %s is in use", id.hex().c_str());
            return false;
        }
    }
    auto it = std::find_if(keys_.begin(), keys_.end(),
                           [&](const KeyMeta& m) { return m.id == id; });
    if (it == keys_.end()) return false;

    std::vector<KeyMeta> new_keys;
    new_keys.reserve(keys_.size() - 1);
    for (const auto& m : keys_) {
        if (!(m.id == id)) new_keys.push_back(m);
    }
    auto snapshot = keys_;
    keys_ = new_keys;
    if (!serialize_to_nvs()) {
        ESP_LOGE(TAG, "delete: nvs serialize failed; rolling back");
        keys_ = snapshot;
        return false;
    }
    // Point of no return; unlink files (ignore ENOENT).
    char path[96];
    std::snprintf(path, sizeof(path), "%s/%s",     KEYS_DIR_PATH, id.hex().c_str());
    unlink(path);
    std::snprintf(path, sizeof(path), "%s/%s.pub", KEYS_DIR_PATH, id.hex().c_str());
    unlink(path);
    return true;
}

bool KeyStore::warn_plaintext_needed() {
    nvs_handle_t h;
    esp_err_t rc = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (rc != ESP_OK) return true;  // conservative: show warning
    uint8_t shown = 0;
    nvs_get_u8(h, NVS_KEY_WARN, &shown);
    if (shown) { nvs_close(h); return false; }
    nvs_set_u8(h, NVS_KEY_WARN, 1);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool KeyStore::serialize_to_nvs() {
    nvs_handle_t h;
    esp_err_t rc = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (rc != ESP_OK) { ESP_LOGE(TAG, "nvs_open: %d", rc); return false; }
    auto blob = index_serialize(keys_);
    rc = nvs_set_blob(h, NVS_KEY_IDX, blob.data(), blob.size());
    if (rc != ESP_OK) { ESP_LOGE(TAG, "nvs_set_blob: %d", rc); nvs_close(h); return false; }
    rc = nvs_commit(h);
    if (rc != ESP_OK) { ESP_LOGE(TAG, "nvs_commit: %d", rc); nvs_close(h); return false; }
    nvs_close(h);
    return true;
}

bool KeyStore::deserialize_from_nvs() {
    nvs_handle_t h;
    esp_err_t rc = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (rc == ESP_ERR_NVS_NOT_FOUND) return true;  // empty = success
    if (rc != ESP_OK) return false;

    size_t len = 0;
    rc = nvs_get_blob(h, NVS_KEY_IDX, nullptr, &len);
    if (rc == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return true; }
    if (rc != ESP_OK) { nvs_close(h); return false; }

    std::vector<uint8_t> blob(len);
    rc = nvs_get_blob(h, NVS_KEY_IDX, blob.data(), &len);
    nvs_close(h);
    if (rc != ESP_OK) return false;

    auto parsed = index_deserialize(blob);
    if (!parsed) return false;
    keys_ = *parsed;
    return true;
}

void KeyStore::sweep_orphans() {
    DIR* d = opendir(KEYS_DIR_PATH);
    if (!d) return;
    std::vector<std::string> to_unlink;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        std::string name = e->d_name;
        // `.tmp` orphans always swept.
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".tmp") {
            to_unlink.push_back(name);
            continue;
        }
        // Strip `.pub` suffix if present.
        std::string id_hex = name;
        if (id_hex.size() >= 4 && id_hex.substr(id_hex.size() - 4) == ".pub") {
            id_hex = id_hex.substr(0, id_hex.size() - 4);
        }
        auto parsed = KeyId::parse(id_hex);
        if (!parsed || !contains(*parsed)) {
            to_unlink.push_back(name);
        }
    }
    closedir(d);
    for (const auto& n : to_unlink) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s/%s", KEYS_DIR_PATH, n.c_str());
        unlink(path);
        ESP_LOGD(TAG, "swept orphan: %s", path);
    }
}

// KeyStore::add() defined in key_codec.cpp (needs libssh to read from ssh_key).

} // namespace ssh_keys
