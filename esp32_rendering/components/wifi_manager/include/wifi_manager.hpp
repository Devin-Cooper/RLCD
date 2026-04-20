#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"

namespace wifi {

enum class State {
    Disconnected,
    Connecting,
    Connected,
};

struct NetworkInfo {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t auth;
};

struct ConnectionInfo {
    State state;
    char ssid[33];
    char ip[16];
    int8_t rssi;
};

using StateCallback = void(*)(State state, void* ctx);

/// Manages WiFi connection lifecycle.
/// Stores known networks in NVS, auto-connects on boot, falls back to scan.
/// Uses nvs_flash_secure_init() for encrypted credential storage.
class WifiManager {
public:
    static constexpr int MAX_KNOWN_NETWORKS = 8;
    static constexpr int MAX_SCAN_RESULTS = 20;

    WifiManager();
    ~WifiManager();

    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    /// Initialize WiFi subsystem. Must be called before any other method.
    void init();

    /// Set callback for state changes.
    void onStateChange(StateCallback cb, void* ctx = nullptr) {
        state_cb_ = cb;
        state_ctx_ = ctx;
    }

    /// Attempt auto-connect to known networks (by signal strength).
    /// Non-blocking — calls state callback on success/failure.
    void autoConnect();

    /// Start a network scan. Results available via scanResults().
    void startScan();

    /// Get scan results (valid after scan completes).
    int scanResults(NetworkInfo* results, int max_results);

    /// Connect to a specific network. Password stored in NVS on success.
    void connect(const char* ssid, const char* password);

    /// Disconnect from current network.
    void disconnect();

    /// Get current connection info.
    ConnectionInfo connectionInfo() const;

    /// Add a network to NVS known list.
    void saveNetwork(const char* ssid, const char* password);

    /// Remove a network from NVS known list.
    void forgetNetwork(const char* ssid);

    /// Populate `out` with up to `max` valid known networks. Returns count.
    int knownNetworks(NetworkInfo* out, int max) const;

    /// Returns true if ssid is in known list and writes the saved password
    /// to `out` (NUL-terminated). Returns false otherwise.
    bool knownPassword(const char* ssid, char* out, size_t out_cap) const;

private:
    std::atomic<State> state_;
    StateCallback state_cb_;
    void* state_ctx_;
    char current_ssid_[33];
    char current_ip_[16];
    int retry_count_;
    int max_retries_;

    // Known networks loaded from NVS
    struct KnownNetwork {
        char ssid[33];
        char password[65];
        bool valid;
    };
    KnownNetwork known_networks_[MAX_KNOWN_NETWORKS];
    int known_count_;

    void loadKnownNetworks();
    void setState(State s);
    int findKnownNetwork(const char* ssid) const;

    // Reconnect timer for non-blocking backoff (avoids blocking event handler)
    esp_timer_handle_t reconnect_timer_;
    static void reconnectTimerCb(void* arg);

    // ESP event handlers
    static void eventHandler(void* arg, esp_event_base_t base,
                              int32_t id, void* data);
    void handleWifiEvent(int32_t id, void* data);
    void handleIpEvent(int32_t id, void* data);
};

} // namespace wifi
