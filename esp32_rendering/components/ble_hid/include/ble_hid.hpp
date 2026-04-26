#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>

// KeyEvent + translateKeycode live in a pure-logic TU so host tests can link
// against them without pulling NimBLE headers.
#include "hid_translate.hpp"

// NimBLE headers needed for callback signatures
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "esp_timer.h"

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

/// Sentinel for invalid GATT attribute handles (UINT16_MAX; conveniently
/// also BLE_HS_CONN_HANDLE_NONE in NimBLE).
static constexpr uint16_t kInvalidHandle = 0xFFFF;

/// One Input-Report characteristic discovered on the peer.
struct ReportSlot {
    uint16_t val_handle  = kInvalidHandle;   // Report value attribute handle
    uint16_t cccd_handle = kInvalidHandle;   // CCCD (0x2902) for this report
    uint8_t  report_id   = 0;                // From 0x2908 Report Reference (0 = implicit)
    uint8_t  type        = 0;                // 0 = unknown/input-implicit, 1 = Input
};

/// BLE HID Host — connects to a Bluetooth keyboard and translates
/// HID key reports to terminal byte sequences.
class BleHidHost {
public:
    BleHidHost();
    ~BleHidHost();

    BleHidHost(const BleHidHost&) = delete;
    BleHidHost& operator=(const BleHidHost&) = delete;

    void init();

    void onKey(KeyCallback cb, void* ctx = nullptr) {
        key_cb_ = cb;
        key_ctx_ = ctx;
    }
    void onStateChange(StateCallback cb, void* ctx = nullptr) {
        state_cb_ = cb;
        state_ctx_ = ctx;
    }

    void startScan();
    void stopScan();
    void connect(const uint8_t addr[6]);
    void startPairing(int timeout_sec = 30);
    void disconnect();
    void autoReconnect();

    State state() const { return state_.load(std::memory_order_acquire); }

    /// True iff NimBLE bond storage has at least one peer-security entry.
    bool hasBond() const;

private:
    std::atomic<State> state_;
    KeyCallback key_cb_;
    void* key_ctx_;
    StateCallback state_cb_;
    void* state_ctx_;

    uint16_t conn_handle_;

    // Report subscription state (Spec 02).
    static constexpr int kMaxReportSlots = 8;
    ReportSlot reports_[kMaxReportSlots];
    int        reports_count_;      // number of slots populated
    int        subscribe_next_idx_; // index of next slot to subscribe to CCCD
    int        pending_ref_reads_;  // outstanding 0x2908 reads (Spec 02 race fix)
    int        current_dsc_slot_;   // index of slot whose descriptors are currently being walked
    bool       post_discovery_done_; // guard so Protocol Mode / CCCD writes only run once

    // Non-Report characteristic handles (0 = not discovered).
    uint16_t protocol_mode_handle_;  // 0x2A4E
    uint16_t control_point_handle_;  // 0x2A4C

    // HID service range for descriptor discovery.
    uint16_t hid_svc_start_handle_;
    uint16_t hid_svc_end_handle_;

    uint8_t prev_keys_[6];

    esp_timer_handle_t pairing_timer_;
    uint8_t last_addr_type_;
    uint8_t own_addr_type_;

    void connectWithType(const uint8_t addr[6], uint8_t addr_type);

    // Pure-logic forward (implemented in hid_report_process.cpp).
    void processHidReport(const uint8_t* report, size_t len, uint8_t report_id);

    /// Translate a single HID keycode + modifiers to a terminal byte sequence.
    KeyEvent translateKeycode(uint8_t keycode, uint8_t modifiers);

    void setState(State s);

    // Pairing timeout callback
    static void pairingTimeoutCb(void* arg);

    // NimBLE sync callback — fires when the controller reports ready.
    static void onSyncStatic();

    // GAP event callback
    static int bleGapEvent(struct ble_gap_event* event, void* arg);

    // GATT discovery callbacks
    static int gattSvcDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                              const struct ble_gatt_svc* service, void* arg);
    static int gattChrDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                              const struct ble_gatt_chr* chr, void* arg);
    static int gattDscDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                              uint16_t chr_val_handle, const struct ble_gatt_dsc* dsc, void* arg);

    // Read-callback for 0x2908 Report Reference descriptors.
    static int reportReferenceReadCb(uint16_t conn_handle,
                                     const struct ble_gatt_error* error,
                                     struct ble_gatt_attr* attr, void* arg);

    // CCCD write completion — chains the next slot's subscription.
    static int cccdWriteCb(uint16_t conn_handle,
                           const struct ble_gatt_error* error,
                           struct ble_gatt_attr* attr, void* arg);

    // Per-phase helpers.
    void startReportDescriptorReads();
    void onAllReferenceReadsComplete();
    void subscribeNextInputReport();
    void writeProtocolMode(uint8_t value);
    void writeControlPoint(uint8_t value);
    void clearReportSlots();

    // Singleton pointer for static sync_cb.
    static BleHidHost* s_instance_;
};

} // namespace ble_hid
