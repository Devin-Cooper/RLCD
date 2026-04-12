#include "ble_hid.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

// NimBLE includes
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include <cstring>

static const char* TAG = "ble_hid";

// HID Service UUID: 0x1812
static const ble_uuid16_t HID_SVC_UUID = BLE_UUID16_INIT(0x1812);
// HID Report characteristic UUID: 0x2A4D
static const ble_uuid16_t HID_REPORT_UUID = BLE_UUID16_INIT(0x2A4D);

namespace ble_hid {

// --- HID keycode to ASCII/terminal byte lookup tables ---

// Standard US keyboard layout: HID usage ID → ASCII (unshifted)
static const char HID_TO_ASCII[128] = {
    0,    0,    0,    0,   'a',  'b',  'c',  'd',  // 0x00-0x07
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  // 0x08-0x0F
    'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  // 0x10-0x17
    'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2',  // 0x18-0x1F
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  // 0x20-0x27
    '\r', 0x1b, 0x7f, '\t', ' ',  '-',  '=',  '[',  // 0x28-0x2F
    ']',  '\\', 0,    ';',  '\'', '`',  ',',  '.',  // 0x30-0x37
    '/',  0,    0,    0,    0,    0,    0,    0,    // 0x38-0x3F (caps, F1-F5)
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x40-0x47 (F6-F12, PrtSc)
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x48-0x4F (ins, home, pgup, del, end, pgdn, right)
    0,    0,    0,    0,    '/',  '*',  '-',  '+',  // 0x50-0x57 (down, up, numlock, kp/)
    '\r', '1',  '2',  '3',  '4',  '5',  '6',  '7',  // 0x58-0x5F (kp enter, kp1-7)
    '8',  '9',  '0',  '.',  0,    0,    0,    0,    // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x78-0x7F
};

// Shifted versions of keys
static const char HID_TO_ASCII_SHIFT[128] = {
    0,    0,    0,    0,   'A',  'B',  'C',  'D',  // 0x00-0x07
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  // 0x08-0x0F
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  // 0x10-0x17
    'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@',  // 0x18-0x1F
    '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  // 0x20-0x27
    '\r', 0x1b, 0x7f, '\t', ' ',  '_',  '+',  '{',  // 0x28-0x2F
    '}',  '|',  0,    ':',  '"',  '~',  '<',  '>',  // 0x30-0x37
    '?',  0,    0,    0,    0,    0,    0,    0,    // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x40-0x47
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x48-0x4F
    0,    0,    0,    0,    '/',  '*',  '-',  '+',  // 0x50-0x57
    '\r', '1',  '2',  '3',  '4',  '5',  '6',  '7',  // 0x58-0x5F
    '8',  '9',  '0',  '.',  0,    0,    0,    0,    // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x78-0x7F
};

// HID modifier bit masks
static constexpr uint8_t MOD_LCTRL  = 0x01;
static constexpr uint8_t MOD_LSHIFT = 0x02;
static constexpr uint8_t MOD_LALT   = 0x04;
static constexpr uint8_t MOD_LGUI   = 0x08;
static constexpr uint8_t MOD_RCTRL  = 0x10;
static constexpr uint8_t MOD_RSHIFT = 0x20;
static constexpr uint8_t MOD_RALT   = 0x40;
static constexpr uint8_t MOD_RGUI   = 0x80;

BleHidHost::BleHidHost()
    : state_(State::Disabled), key_cb_(nullptr), key_ctx_(nullptr),
      state_cb_(nullptr), state_ctx_(nullptr),
      conn_handle_(0xFFFF), hid_report_handle_(0),
      pairing_timer_(nullptr) {
    std::memset(prev_keys_, 0, sizeof(prev_keys_));
}

BleHidHost::~BleHidHost() {
    if (pairing_timer_) {
        esp_timer_stop(static_cast<esp_timer_handle_t>(pairing_timer_));
        esp_timer_delete(static_cast<esp_timer_handle_t>(pairing_timer_));
    }
}

void BleHidHost::init() {
    // NimBLE initialization is typically done in the main app
    // This just sets up our state
    setState(State::Disconnected);
    ESP_LOGI(TAG, "BLE HID Host initialized");
}

void BleHidHost::startScan() {
    struct ble_gap_disc_params disc_params = {};
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER,
                           &disc_params, bleGapEvent, this);
    if (rc == 0) {
        setState(State::Scanning);
        ESP_LOGI(TAG, "Started BLE scan");
    } else {
        ESP_LOGE(TAG, "Failed to start scan: %d", rc);
    }
}

void BleHidHost::stopScan() {
    ble_gap_disc_cancel();
    if (state_ == State::Scanning) {
        setState(State::Disconnected);
    }
}

void BleHidHost::connect(const uint8_t addr[6]) {
    ble_addr_t peer_addr;
    peer_addr.type = BLE_ADDR_PUBLIC;
    std::memcpy(peer_addr.val, addr, 6);

    stopScan();

    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer_addr,
                              30000, nullptr, bleGapEvent, this);
    if (rc == 0) {
        setState(State::Connecting);
        ESP_LOGI(TAG, "Connecting to %02x:%02x:%02x:%02x:%02x:%02x",
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    } else {
        ESP_LOGE(TAG, "Failed to connect: %d", rc);
    }
}

void BleHidHost::startPairing(int timeout_sec) {
    ESP_LOGI(TAG, "Entering pairing mode (%ds timeout)", timeout_sec);
    startScan();

    // Set up timeout timer
    if (!pairing_timer_) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = pairingTimeoutCb;
        timer_args.arg = this;
        timer_args.name = "ble_pair_timeout";
        esp_timer_create(&timer_args,
                          reinterpret_cast<esp_timer_handle_t*>(&pairing_timer_));
    }
    esp_timer_start_once(static_cast<esp_timer_handle_t>(pairing_timer_),
                          static_cast<uint64_t>(timeout_sec) * 1000000ULL);
}

void BleHidHost::disconnect() {
    if (conn_handle_ != 0xFFFF) {
        ble_gap_terminate(conn_handle_, BLE_ERR_REM_USER_CONN_TERM);
    }
    setState(State::Disconnected);
}

void BleHidHost::autoReconnect() {
    // NimBLE stores bonding info and can auto-reconnect
    // For now, start a scan and connect to any known bonded device
    ESP_LOGI(TAG, "Attempting auto-reconnect to bonded keyboard");
    startScan();
}

// --- HID Report Processing ---

void BleHidHost::processHidReport(const uint8_t* report, size_t len) {
    if (len < 8) return;

    uint8_t modifiers = report[0];
    // report[1] is reserved

    // Detect newly pressed keys (present in current report but not previous)
    for (int i = 2; i < 8; i++) {
        uint8_t key = report[i];
        if (key == 0) continue;

        // Check if this key was already in the previous report
        bool was_pressed = false;
        for (int j = 2; j < 8; j++) {
            if (prev_keys_[j - 2] == key) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed) {
            KeyEvent event = translateKeycode(key, modifiers);
            if (event.length > 0 && key_cb_) {
                key_cb_(event, key_ctx_);
            }
        }
    }

    // Save current key state
    std::memcpy(prev_keys_, report + 2, 6);
}

KeyEvent BleHidHost::translateKeycode(uint8_t keycode, uint8_t modifiers) {
    KeyEvent event = {};
    bool shift = (modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;
    bool ctrl = (modifiers & (MOD_LCTRL | MOD_RCTRL)) != 0;

    // Arrow keys → ANSI escape sequences
    switch (keycode) {
    case 0x4F: // Right arrow
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = 'C';
        event.length = 3;
        return event;
    case 0x50: // Left arrow
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = 'D';
        event.length = 3;
        return event;
    case 0x51: // Down arrow
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = 'B';
        event.length = 3;
        return event;
    case 0x52: // Up arrow
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = 'A';
        event.length = 3;
        return event;

    // Navigation keys
    case 0x49: // Insert
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '2'; event.bytes[3] = '~';
        event.length = 4;
        return event;
    case 0x4C: // Delete
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '3'; event.bytes[3] = '~';
        event.length = 4;
        return event;
    case 0x4A: // Home
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = 'H';
        event.length = 3;
        return event;
    case 0x4D: // End
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = 'F';
        event.length = 3;
        return event;
    case 0x4B: // Page Up
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '5'; event.bytes[3] = '~';
        event.length = 4;
        return event;
    case 0x4E: // Page Down
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '6'; event.bytes[3] = '~';
        event.length = 4;
        return event;

    // Function keys F1-F12 → xterm sequences
    case 0x3A: // F1
        event.bytes[0] = 0x1B; event.bytes[1] = 'O'; event.bytes[2] = 'P';
        event.length = 3;
        return event;
    case 0x3B: // F2
        event.bytes[0] = 0x1B; event.bytes[1] = 'O'; event.bytes[2] = 'Q';
        event.length = 3;
        return event;
    case 0x3C: // F3
        event.bytes[0] = 0x1B; event.bytes[1] = 'O'; event.bytes[2] = 'R';
        event.length = 3;
        return event;
    case 0x3D: // F4
        event.bytes[0] = 0x1B; event.bytes[1] = 'O'; event.bytes[2] = 'S';
        event.length = 3;
        return event;
    case 0x3E: // F5
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '1';
        event.bytes[3] = '5'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x3F: // F6
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '1';
        event.bytes[3] = '7'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x40: // F7
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '1';
        event.bytes[3] = '8'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x41: // F8
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '1';
        event.bytes[3] = '9'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x42: // F9
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '2';
        event.bytes[3] = '0'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x43: // F10
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '2';
        event.bytes[3] = '1'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x44: // F11
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '2';
        event.bytes[3] = '3'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    case 0x45: // F12
        event.bytes[0] = 0x1B; event.bytes[1] = '['; event.bytes[2] = '2';
        event.bytes[3] = '4'; event.bytes[4] = '~';
        event.length = 5;
        return event;
    }

    // Regular keys via lookup table
    if (keycode < 128) {
        char ch;
        if (ctrl && keycode >= 0x04 && keycode <= 0x1D) {
            // Ctrl+letter → control code (0x01-0x1A)
            ch = static_cast<char>(keycode - 0x04 + 1);
        } else if (shift) {
            ch = HID_TO_ASCII_SHIFT[keycode];
        } else {
            ch = HID_TO_ASCII[keycode];
        }

        if (ch != 0) {
            event.bytes[0] = static_cast<uint8_t>(ch);
            event.length = 1;
        }
    }

    return event;
}

// --- State ---

void BleHidHost::setState(State s) {
    state_ = s;
    if (state_cb_) {
        state_cb_(s, state_ctx_);
    }
}

void BleHidHost::pairingTimeoutCb(void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);
    if (self->state_ == State::Scanning) {
        ESP_LOGW(TAG, "Pairing timeout — exiting pairing mode");
        self->stopScan();
    }
}

// --- NimBLE GAP Event Handler ---

int BleHidHost::bleGapEvent(void* event_ptr, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);
    auto* event = static_cast<struct ble_gap_event*>(event_ptr);

    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        // Check if the discovered device advertises HID service
        struct ble_hs_adv_fields fields;
        ble_hs_adv_parse_fields(&fields, event->disc.data,
                                 event->disc.length_data);

        bool has_hid = false;
        for (int i = 0; i < fields.num_uuids16; i++) {
            if (ble_uuid_cmp(&fields.uuids16[i].u, &HID_SVC_UUID.u) == 0) {
                has_hid = true;
                break;
            }
        }

        if (has_hid) {
            ESP_LOGI(TAG, "Found HID device, connecting...");
            // Stop pairing timer if running
            if (self->pairing_timer_) {
                esp_timer_stop(static_cast<esp_timer_handle_t>(self->pairing_timer_));
            }
            self->connect(event->disc.addr.val);
        }
        break;
    }

    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            self->conn_handle_ = event->connect.conn_handle;
            self->setState(State::Connected);
            ESP_LOGI(TAG, "Connected (handle=%d)", self->conn_handle_);

            // Discover HID service and subscribe to reports
            // In a full implementation, this would:
            // 1. ble_gattc_disc_all_svcs() to find HID service
            // 2. ble_gattc_disc_all_chrs() to find Report characteristic
            // 3. ble_gattc_subscribe() to enable notifications
            // The notification callback would call processHidReport()
        } else {
            ESP_LOGW(TAG, "Connection failed: %d", event->connect.status);
            self->setState(State::Disconnected);
        }
        break;
    }

    case BLE_GAP_EVENT_DISCONNECT: {
        self->conn_handle_ = 0xFFFF;
        self->setState(State::Disconnected);
        ESP_LOGW(TAG, "Disconnected (reason=%d)", event->disconnect.reason);
        break;
    }

    case BLE_GAP_EVENT_NOTIFY_RX: {
        // HID report received via notification
        if (event->notify_rx.attr_handle == self->hid_report_handle_) {
            self->processHidReport(
                OS_MBUF_DATA(event->notify_rx.om, uint8_t*),
                OS_MBUF_PKTLEN(event->notify_rx.om));
        }
        break;
    }

    default:
        break;
    }

    return 0;
}

} // namespace ble_hid
