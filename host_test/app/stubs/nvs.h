#pragma once
#include "esp_err.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <set>
#include <string>

constexpr int ESP_ERR_NVS_NOT_FOUND = 1;
constexpr int ESP_ERR_NVS_INVALID_LENGTH = 2;
using nvs_handle_t = uint32_t;

enum nvs_open_mode_t { NVS_READONLY = 0, NVS_READWRITE = 1 };

namespace nvs_stub {
    inline std::map<std::string, std::map<std::string, std::string>>& db() {
        static std::map<std::string, std::map<std::string, std::string>> d;
        return d;
    }
    inline std::set<std::string>& existing_nss() {
        static std::set<std::string> s;
        return s;
    }
    inline std::map<nvs_handle_t, std::string>& handle_ns() {
        static std::map<nvs_handle_t, std::string> h;
        return h;
    }
    inline std::string cur_ns(nvs_handle_t h) {
        auto it = handle_ns().find(h);
        return (it == handle_ns().end()) ? "" : it->second;
    }
    inline void set_cur_ns(nvs_handle_t h, const std::string& ns) {
        handle_ns()[h] = ns;
    }
    inline void clear_cur_ns(nvs_handle_t h) {
        handle_ns().erase(h);
    }
    inline nvs_handle_t next_handle() {
        static nvs_handle_t n = 1;
        return n++;
    }
}

inline esp_err_t nvs_open(const char* ns, nvs_open_mode_t mode, nvs_handle_t* out) {
    if (!ns || !out) return 1;
    // Amendment E: READONLY of a non-existent namespace returns NOT_FOUND.
    if (mode == NVS_READONLY && nvs_stub::existing_nss().count(ns) == 0) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *out = nvs_stub::next_handle();
    nvs_stub::set_cur_ns(*out, ns);
    if (mode == NVS_READWRITE) nvs_stub::existing_nss().insert(ns);
    return ESP_OK;
}
inline void nvs_close(nvs_handle_t h) { nvs_stub::clear_cur_ns(h); }
inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }

inline esp_err_t nvs_set_u8(nvs_handle_t h, const char* k, uint8_t v) {
    nvs_stub::db()[nvs_stub::cur_ns(h)][k] = std::string(1, static_cast<char>(v));
    return ESP_OK;
}
inline esp_err_t nvs_set_u16(nvs_handle_t h, const char* k, uint16_t v) {
    char buf[2];
    buf[0] = static_cast<char>(v & 0xff);
    buf[1] = static_cast<char>((v >> 8) & 0xff);
    nvs_stub::db()[nvs_stub::cur_ns(h)][k] = std::string(buf, 2);
    return ESP_OK;
}
inline esp_err_t nvs_set_str(nvs_handle_t h, const char* k, const char* v) {
    nvs_stub::db()[nvs_stub::cur_ns(h)][k] = v ? v : "";
    return ESP_OK;
}
inline esp_err_t nvs_get_u8(nvs_handle_t h, const char* k, uint8_t* v) {
    auto& ns = nvs_stub::db()[nvs_stub::cur_ns(h)];
    auto it = ns.find(k);
    if (it == ns.end() || it->second.empty()) return ESP_ERR_NVS_NOT_FOUND;
    *v = static_cast<uint8_t>(it->second[0]);
    return ESP_OK;
}
inline esp_err_t nvs_get_u16(nvs_handle_t h, const char* k, uint16_t* v) {
    auto& ns = nvs_stub::db()[nvs_stub::cur_ns(h)];
    auto it = ns.find(k);
    if (it == ns.end() || it->second.size() < 2) return ESP_ERR_NVS_NOT_FOUND;
    *v = static_cast<uint8_t>(it->second[0]) |
         (static_cast<uint8_t>(it->second[1]) << 8);
    return ESP_OK;
}
inline esp_err_t nvs_get_str(nvs_handle_t h, const char* k, char* out, size_t* len) {
    auto& ns = nvs_stub::db()[nvs_stub::cur_ns(h)];
    auto it = ns.find(k);
    if (it == ns.end()) return ESP_ERR_NVS_NOT_FOUND;
    size_t n = it->second.size();
    if (n + 1 > *len) { *len = n + 1; return ESP_ERR_NVS_INVALID_LENGTH; }
    std::memcpy(out, it->second.data(), n);
    out[n] = '\0';
    *len = n + 1;
    return ESP_OK;
}
inline esp_err_t nvs_erase_key(nvs_handle_t h, const char* k) {
    nvs_stub::db()[nvs_stub::cur_ns(h)].erase(k);
    return ESP_OK;
}
inline esp_err_t nvs_erase_all(nvs_handle_t h) {
    nvs_stub::db()[nvs_stub::cur_ns(h)].clear();
    return ESP_OK;
}
