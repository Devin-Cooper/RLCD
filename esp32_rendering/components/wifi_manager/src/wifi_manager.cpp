#include "wifi_manager.hpp"
#include "input_queue.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <algorithm>

static const char* TAG = "wifi_mgr";
static const char* NVS_NAMESPACE = "wifi_creds";

namespace wifi {

WifiManager::WifiManager()
    : state_(State::Disconnected), state_cb_(nullptr), state_ctx_(nullptr),
      retry_count_(0), max_retries_(10), known_count_(0),
      reconnect_timer_(nullptr) {
    std::memset(current_ssid_, 0, sizeof(current_ssid_));
    std::memset(current_ip_, 0, sizeof(current_ip_));
    std::memset(known_networks_, 0, sizeof(known_networks_));
}

WifiManager::~WifiManager() {
    if (reconnect_timer_) {
        esp_timer_stop(reconnect_timer_);
        esp_timer_delete(reconnect_timer_);
    }
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler);
}

void WifiManager::init() {
    // NVS must be initialized externally (with nvs_flash_secure_init for encryption)

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, this));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Create reconnect timer for non-blocking backoff retries
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = reconnectTimerCb;
    timer_args.arg = this;
    timer_args.name = "wifi_reconnect";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &reconnect_timer_));

    loadKnownNetworks();
    ESP_LOGI(TAG, "WiFi initialized, %d known networks", known_count_);
}

void WifiManager::autoConnect() {
    if (known_count_ == 0) {
        ESP_LOGI(TAG, "No known networks, starting scan");
        startScan();
        return;
    }

    // Scan first to find available networks
    ESP_LOGI(TAG, "Scanning for known networks...");
    esp_wifi_scan_start(nullptr, true); // blocking scan

    uint16_t num = MAX_SCAN_RESULTS;
    wifi_ap_record_t records[MAX_SCAN_RESULTS];
    esp_wifi_scan_get_ap_records(&num, records);

    // Find best known network by signal strength
    int best_idx = -1;
    int8_t best_rssi = -128;

    for (int i = 0; i < num; i++) {
        int known_idx = findKnownNetwork(reinterpret_cast<const char*>(records[i].ssid));
        if (known_idx >= 0 && records[i].rssi > best_rssi) {
            best_idx = known_idx;
            best_rssi = records[i].rssi;
        }
    }

    if (best_idx >= 0) {
        ESP_LOGI(TAG, "Auto-connecting to '%s' (RSSI %d)",
                 known_networks_[best_idx].ssid, best_rssi);
        connect(known_networks_[best_idx].ssid,
                known_networks_[best_idx].password);
    } else {
        ESP_LOGI(TAG, "No known networks found in scan");
        setState(State::Disconnected);
    }
}

void WifiManager::startScan() {
    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = false;
    esp_wifi_scan_start(&scan_config, false); // non-blocking
}

int WifiManager::scanResults(NetworkInfo* results, int max_results) {
    uint16_t num = max_results;
    wifi_ap_record_t* records = new wifi_ap_record_t[num];
    esp_wifi_scan_get_ap_records(&num, records);

    int count = std::min(static_cast<int>(num), max_results);
    for (int i = 0; i < count; i++) {
        std::strncpy(results[i].ssid,
                     reinterpret_cast<const char*>(records[i].ssid),
                     sizeof(results[i].ssid) - 1);
        results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
        results[i].rssi = records[i].rssi;
        results[i].auth = records[i].authmode;
    }

    delete[] records;
    return count;
}

void WifiManager::connect(const char* ssid, const char* password) {
    wifi_config_t config = {};
    std::strncpy(reinterpret_cast<char*>(config.sta.ssid), ssid,
                 sizeof(config.sta.ssid) - 1);
    if (password && password[0]) {
        std::strncpy(reinterpret_cast<char*>(config.sta.password), password,
                     sizeof(config.sta.password) - 1);
    }
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    std::strncpy(current_ssid_, ssid, sizeof(current_ssid_) - 1);
    retry_count_ = 0;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    esp_wifi_connect();
    setState(State::Connecting);
}

void WifiManager::disconnect() {
    esp_wifi_disconnect();
    setState(State::Disconnected);
}

ConnectionInfo WifiManager::connectionInfo() const {
    ConnectionInfo info = {};
    info.state = state_.load(std::memory_order_acquire);
    std::strncpy(info.ssid, current_ssid_, sizeof(info.ssid) - 1);
    std::strncpy(info.ip, current_ip_, sizeof(info.ip) - 1);

    if (info.state == State::Connected) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            info.rssi = ap.rssi;
        }
    }
    return info;
}

void WifiManager::saveNetwork(const char* ssid, const char* password) {
    // Check if already known
    int idx = findKnownNetwork(ssid);
    if (idx < 0) {
        // Find empty slot
        for (int i = 0; i < MAX_KNOWN_NETWORKS; i++) {
            if (!known_networks_[i].valid) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            // Overwrite oldest (slot 0)
            idx = 0;
        }
    }

    std::strncpy(known_networks_[idx].ssid, ssid,
                 sizeof(known_networks_[idx].ssid) - 1);
    std::strncpy(known_networks_[idx].password, password,
                 sizeof(known_networks_[idx].password) - 1);
    known_networks_[idx].valid = true;

    // Persist to NVS
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        char key_ssid[16], key_pass[16];
        snprintf(key_ssid, sizeof(key_ssid), "ssid_%d", idx);
        snprintf(key_pass, sizeof(key_pass), "pass_%d", idx);
        nvs_set_str(handle, key_ssid, ssid);
        nvs_set_str(handle, key_pass, password);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Saved network '%s' at slot %d", ssid, idx);
    }

    known_count_ = 0;
    for (int i = 0; i < MAX_KNOWN_NETWORKS; i++) {
        if (known_networks_[i].valid) known_count_++;
    }
}

void WifiManager::forgetNetwork(const char* ssid) {
    int idx = findKnownNetwork(ssid);
    if (idx < 0) return;

    known_networks_[idx].valid = false;
    std::memset(known_networks_[idx].ssid, 0, sizeof(known_networks_[idx].ssid));
    std::memset(known_networks_[idx].password, 0, sizeof(known_networks_[idx].password));

    // Remove from NVS
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        char key_ssid[16], key_pass[16];
        snprintf(key_ssid, sizeof(key_ssid), "ssid_%d", idx);
        snprintf(key_pass, sizeof(key_pass), "pass_%d", idx);
        nvs_erase_key(handle, key_ssid);
        nvs_erase_key(handle, key_pass);
        nvs_commit(handle);
        nvs_close(handle);
    }

    known_count_ = 0;
    for (int i = 0; i < MAX_KNOWN_NETWORKS; i++) {
        if (known_networks_[i].valid) known_count_++;
    }
}

int WifiManager::knownNetworks(NetworkInfo* out, int max) const {
    int n = 0;
    for (int i = 0; i < MAX_KNOWN_NETWORKS && n < max; ++i) {
        if (!known_networks_[i].valid) continue;
        std::strncpy(out[n].ssid, known_networks_[i].ssid, sizeof(out[n].ssid) - 1);
        out[n].ssid[sizeof(out[n].ssid) - 1] = '\0';
        out[n].rssi = 0;
        out[n].auth = WIFI_AUTH_WPA2_PSK;   // unknown at this level; default secured
        ++n;
    }
    return n;
}

bool WifiManager::knownPassword(const char* ssid, char* out, size_t out_cap) const {
    int idx = findKnownNetwork(ssid);
    if (idx < 0 || !out || out_cap == 0) return false;
    std::strncpy(out, known_networks_[idx].password, out_cap - 1);
    out[out_cap - 1] = '\0';
    return true;
}

// --- Private ---

void WifiManager::loadKnownNetworks() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "No NVS namespace for WiFi credentials");
        return;
    }

    known_count_ = 0;
    for (int i = 0; i < MAX_KNOWN_NETWORKS; i++) {
        char key_ssid[16], key_pass[16];
        snprintf(key_ssid, sizeof(key_ssid), "ssid_%d", i);
        snprintf(key_pass, sizeof(key_pass), "pass_%d", i);

        size_t ssid_len = sizeof(known_networks_[i].ssid);
        size_t pass_len = sizeof(known_networks_[i].password);

        if (nvs_get_str(handle, key_ssid, known_networks_[i].ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(handle, key_pass, known_networks_[i].password, &pass_len) == ESP_OK) {
            known_networks_[i].valid = true;
            known_count_++;
        }
    }

    nvs_close(handle);
}

void WifiManager::setState(State s) {
    state_.store(s, std::memory_order_release);
    if (state_cb_) {
        state_cb_(s, state_ctx_);
    }
}

int WifiManager::findKnownNetwork(const char* ssid) const {
    for (int i = 0; i < MAX_KNOWN_NETWORKS; i++) {
        if (known_networks_[i].valid &&
            std::strncmp(known_networks_[i].ssid, ssid, 32) == 0) {
            return i;
        }
    }
    return -1;
}

void WifiManager::eventHandler(void* arg, esp_event_base_t base,
                                int32_t id, void* data) {
    auto* self = static_cast<WifiManager*>(arg);
    if (base == WIFI_EVENT) {
        self->handleWifiEvent(id, data);
    } else if (base == IP_EVENT) {
        self->handleIpEvent(id, data);
    }
}

void WifiManager::handleWifiEvent(int32_t id, void* data) {
    switch (id) {
    case WIFI_EVENT_STA_DISCONNECTED: {
        ESP_LOGW(TAG, "Disconnected from WiFi");
        if (state_.load(std::memory_order_acquire) == State::Connecting && retry_count_ < max_retries_) {
            // Reconnect with backoff: 1s, 2s, 4s, 8s, 16s, 30s (capped)
            // Uses a one-shot timer to avoid blocking the event handler
            int delay_ms = std::min(1000 << retry_count_, 30000);
            ESP_LOGI(TAG, "Retry %d/%d in %dms", retry_count_ + 1, max_retries_, delay_ms);
            retry_count_++;
            esp_timer_start_once(reconnect_timer_,
                                  static_cast<uint64_t>(delay_ms) * 1000ULL);
        } else {
            setState(State::Disconnected);
        }
        break;
    }
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Connected to WiFi (waiting for IP)");
        break;
    case WIFI_EVENT_SCAN_DONE: {
        input::InputEvent ie{};
        ie.source = input::Source::System;
        ie.type   = input::EventType::WifiScanDone;
        ie.data_length = 0;
        input::globalInputQueue().pushOrDrop(ie);
        break;
    }
    default:
        break;
    }
}

void WifiManager::reconnectTimerCb(void* arg) {
    (void)arg;
    esp_wifi_connect();
}

void WifiManager::handleIpEvent(int32_t id, void* data) {
    if (id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);
        snprintf(current_ip_, sizeof(current_ip_), IPSTR,
                 IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", current_ip_);
        retry_count_ = 0;

        // Save this network if not already known
        if (findKnownNetwork(current_ssid_) < 0) {
            // We'd need the password here — it's already set in the wifi config
            // For auto-save, we save during connect() call instead
        }

        setState(State::Connected);
    }
}

} // namespace wifi
