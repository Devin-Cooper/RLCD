#pragma once
#include <cstdint>
using wifi_auth_mode_t = int;
constexpr int WIFI_AUTH_OPEN = 0;
constexpr int WIFI_AUTH_WPA2_PSK = 2;

struct wifi_ap_record_t { uint8_t ssid[33]; int8_t rssi; int authmode; };
inline int esp_wifi_connect() { return 0; }
inline int esp_wifi_disconnect() { return 0; }
struct wifi_scan_config_t { int show_hidden; };
inline int esp_wifi_scan_start(const wifi_scan_config_t*, bool) { return 0; }
inline int esp_wifi_scan_get_ap_records(uint16_t*, wifi_ap_record_t*) { return 0; }
