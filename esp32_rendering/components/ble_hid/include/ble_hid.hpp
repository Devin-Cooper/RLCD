#pragma once

#include <cstdint>
#include <cstddef>

// KeyEvent + translateKeycode live in a pure-logic TU so host tests can link
// against them without pulling NimBLE headers.
#include "hid_translate.hpp"

// NimBLE headers needed for callback signatures
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

namespace ble_hid {

enum class State {
    Disabled,
    Scanning,
    Connecting,
    Connected,
    Disconnected,
};

using KeyCallback = void(*)(const KeyEvent& event, void* ctx);
using StateCallback = void(*)(State state, void* ctx);

/// BLE HID Host — connects to a Bluetooth keyboard and translates
/// HID key reports to terminal byte sequences.
///
/// Uses NimBLE (ESP-IDF's BLE host stack) as a GATT client connecting
/// to a BLE keyboard's HID service (UUID 0x1812).
class BleHidHost {
public:
    BleHidHost();
    ~BleHidHost();

    BleHidHost(const BleHidHost&) = delete;
    BleHidHost& operator=(const BleHidHost&) = delete;

    /// Initialize BLE stack (NimBLE host).
    void init();

    /// Set callbacks.
    void onKey(KeyCallback cb, void* ctx = nullptr) {
        key_cb_ = cb;
        key_ctx_ = ctx;
    }
    void onStateChange(StateCallback cb, void* ctx = nullptr) {
        state_cb_ = cb;
        state_ctx_ = ctx;
    }

    /// Start scanning for HID devices.
    void startScan();

    /// Stop scanning.
    void stopScan();

    /// Connect to a specific device by address.
    void connect(const uint8_t addr[6]);

    /// Start pairing mode (scan + auto-connect to first HID keyboard found).
    /// Automatically exits pairing mode after timeout_sec seconds (default 30).
    void startPairing(int timeout_sec = 30);

    /// Disconnect from current device.
    void disconnect();

    /// Attempt reconnect to last bonded device (non-blocking).
    void autoReconnect();

    State state() const { return state_; }

private:
    State state_;
    KeyCallback key_cb_;
    void* key_ctx_;
    StateCallback state_cb_;
    void* state_ctx_;

    uint16_t conn_handle_;
    uint16_t hid_report_handle_;
    uint8_t prev_keys_[6];  // Previous key state for press/release detection

    // Pairing timeout timer handle
    void* pairing_timer_;

    // Last discovered address type (public, random, etc.)
    uint8_t last_addr_type_;

    /// Connect using a specific address type.
    void connectWithType(const uint8_t addr[6], uint8_t addr_type);

    /// Process a HID boot keyboard report (8 bytes).
    /// Format: [modifier, reserved, key1, key2, key3, key4, key5, key6]
    void processHidReport(const uint8_t* report, size_t len);

    /// Translate a single HID keycode + modifiers to a terminal byte sequence.
    KeyEvent translateKeycode(uint8_t keycode, uint8_t modifiers);

    void setState(State s);

    // Pairing timeout callback
    static void pairingTimeoutCb(void* arg);

    // NimBLE callbacks (static, registered with the stack)
    static int bleGapEvent(struct ble_gap_event* event, void* arg);
    static int gattSvcDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                              const struct ble_gatt_svc* service, void* arg);
    static int gattChrDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                              const struct ble_gatt_chr* chr, void* arg);
    static int gattDscDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                              uint16_t chr_val_handle, const struct ble_gatt_dsc* dsc, void* arg);
};

} // namespace ble_hid
