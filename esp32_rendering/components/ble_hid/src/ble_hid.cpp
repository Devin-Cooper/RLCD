#include "ble_hid.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

// NimBLE includes
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "store/config/ble_store_config.h"
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
      pairing_timer_(nullptr),
      last_addr_type_(0) {
    std::memset(prev_keys_, 0, sizeof(prev_keys_));
}

BleHidHost::~BleHidHost() {
    if (pairing_timer_) {
        esp_timer_stop(static_cast<esp_timer_handle_t>(pairing_timer_));
        esp_timer_delete(static_cast<esp_timer_handle_t>(pairing_timer_));
    }
}

void BleHidHost::init() {
    // Initialize NimBLE host stack
    ESP_ERROR_CHECK(nimble_port_init());

    // Configure NimBLE host
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_sc = 1;

    // Start NimBLE host task
    nimble_port_freertos_init([](void*) {
        nimble_port_run();
        nimble_port_freertos_deinit();
    });

    // Give the host stack a moment to start
    vTaskDelay(pdMS_TO_TICKS(200));

    setState(State::Disconnected);
    ESP_LOGI(TAG, "BLE HID Host initialized (NimBLE started)");
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
    // Use stored address type from last discovery
    connectWithType(addr, last_addr_type_);
}

void BleHidHost::connectWithType(const uint8_t addr[6], uint8_t addr_type) {
    ble_addr_t peer_addr;
    peer_addr.type = addr_type;
    std::memcpy(peer_addr.val, addr, 6);

    stopScan();

    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer_addr,
                              30000, nullptr, bleGapEvent, this);
    if (rc == 0) {
        setState(State::Connecting);
        ESP_LOGI(TAG, "Connecting to %02x:%02x:%02x:%02x:%02x:%02x (type=%d)",
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr_type);
    } else {
        ESP_LOGE(TAG, "Failed to connect: %d (addr_type=%d)", rc, addr_type);
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
    // Try direct connection to bonded peers first (faster than scanning)
    int num_peers = 0;
    ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &num_peers);
    if (num_peers > 0) {
        ble_addr_t peer_addrs[4];
        int count = 0;
        ble_store_util_bonded_peers(peer_addrs, &count, 4);
        if (count > 0) {
            ESP_LOGI(TAG, "Attempting direct connect to bonded peer "
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     peer_addrs[0].val[0], peer_addrs[0].val[1],
                     peer_addrs[0].val[2], peer_addrs[0].val[3],
                     peer_addrs[0].val[4], peer_addrs[0].val[5]);
            connect(peer_addrs[0].val);
            return;
        }
    }
    // Fall back to scanning if no bonded peers
    ESP_LOGI(TAG, "No bonded peers, starting scan");
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

// --- GATT Discovery Callbacks ---

// Step 1: Discover all services — look for HID (0x1812)
int BleHidHost::gattSvcDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                               const struct ble_gatt_svc* service, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Service discovery complete");
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Service discovery error: %d", error->status);
        return 0;
    }

    // Check if this is the HID service
    if (ble_uuid_cmp(&service->uuid.u, &HID_SVC_UUID.u) == 0) {
        ESP_LOGI(TAG, "Found HID service (handle %d-%d)",
                 service->start_handle, service->end_handle);
        // Discover characteristics within HID service
        int rc = ble_gattc_disc_all_chrs(conn_handle,
                                          service->start_handle,
                                          service->end_handle,
                                          gattChrDiscCb, self);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to discover characteristics: %d", rc);
        }
    }
    return 0;
}

// Step 2: Discover characteristics — look for Report (0x2A4D)
int BleHidHost::gattChrDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                               const struct ble_gatt_chr* chr, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Characteristic discovery complete");
        // Now subscribe to notifications on the report handle we found
        if (self->hid_report_handle_ != 0) {
            ESP_LOGI(TAG, "Subscribing to HID report notifications (handle=%d)",
                     self->hid_report_handle_);
            // Write 0x0001 to the CCCD (Client Characteristic Configuration Descriptor)
            // The CCCD is at handle + 1
            uint8_t notify_enable[] = {0x01, 0x00};  // Enable notifications
            int rc = ble_gattc_write_flat(conn_handle,
                                           self->hid_report_handle_ + 1,
                                           notify_enable, sizeof(notify_enable),
                                           nullptr, nullptr);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to enable notifications: %d", rc);
                // Try descriptor discovery instead
                ESP_LOGI(TAG, "Trying descriptor discovery...");
                ble_gattc_disc_all_dscs(conn_handle,
                                         self->hid_report_handle_,
                                         self->hid_report_handle_ + 5,
                                         gattDscDiscCb, self);
            } else {
                ESP_LOGI(TAG, "Notifications enabled — keyboard ready");
            }
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Characteristic discovery error: %d", error->status);
        return 0;
    }

    // Check if this is a Report characteristic (0x2A4D)
    if (ble_uuid_cmp(&chr->uuid.u, &HID_REPORT_UUID.u) == 0) {
        ESP_LOGI(TAG, "Found HID Report characteristic (val_handle=%d, props=0x%02x)",
                 chr->val_handle, chr->properties);
        // We want the one with Notify property (input report)
        if (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) {
            self->hid_report_handle_ = chr->val_handle;
            ESP_LOGI(TAG, "Selected input report handle: %d", chr->val_handle);
        }
    }
    return 0;
}

// Step 3 (fallback): Discover descriptors to find CCCD
int BleHidHost::gattDscDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                               uint16_t chr_val_handle, const struct ble_gatt_dsc* dsc,
                               void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == BLE_HS_EDONE) {
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Descriptor discovery error: %d", error->status);
        return 0;
    }

    // Look for CCCD (UUID 0x2902)
    ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);
    if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) == 0) {
        ESP_LOGI(TAG, "Found CCCD at handle %d — enabling notifications", dsc->handle);
        uint8_t notify_enable[] = {0x01, 0x00};
        ble_gattc_write_flat(conn_handle, dsc->handle,
                              notify_enable, sizeof(notify_enable),
                              nullptr, nullptr);
    }
    return 0;
}

// --- NimBLE GAP Event Handler ---

int BleHidHost::bleGapEvent(struct ble_gap_event* event, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

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
            // Log device name if available
            if (fields.name_len > 0) {
                char name[32] = {};
                int nlen = fields.name_len < 31 ? fields.name_len : 31;
                memcpy(name, fields.name, nlen);
                ESP_LOGI(TAG, "Found HID device: '%s' addr=%02x:%02x:%02x:%02x:%02x:%02x type=%d rssi=%d",
                         name,
                         event->disc.addr.val[0], event->disc.addr.val[1],
                         event->disc.addr.val[2], event->disc.addr.val[3],
                         event->disc.addr.val[4], event->disc.addr.val[5],
                         event->disc.addr.type, event->disc.rssi);
            } else {
                ESP_LOGI(TAG, "Found HID device: addr=%02x:%02x:%02x:%02x:%02x:%02x type=%d rssi=%d",
                         event->disc.addr.val[0], event->disc.addr.val[1],
                         event->disc.addr.val[2], event->disc.addr.val[3],
                         event->disc.addr.val[4], event->disc.addr.val[5],
                         event->disc.addr.type, event->disc.rssi);
            }

            // Stop pairing timer if running
            if (self->pairing_timer_) {
                esp_timer_stop(static_cast<esp_timer_handle_t>(self->pairing_timer_));
            }
            // Save address type and connect
            self->last_addr_type_ = event->disc.addr.type;
            self->connectWithType(event->disc.addr.val, event->disc.addr.type);
        }
        break;
    }

    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            self->conn_handle_ = event->connect.conn_handle;
            self->setState(State::Connected);
            ESP_LOGI(TAG, "Connected (handle=%d)", self->conn_handle_);

            // Start GATT service discovery to find HID Report characteristic
            ESP_LOGI(TAG, "Starting GATT service discovery...");
            int rc2 = ble_gattc_disc_all_svcs(self->conn_handle_, gattSvcDiscCb, self);
            if (rc2 != 0) {
                ESP_LOGE(TAG, "Failed to start service discovery: %d", rc2);
            }
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
        uint16_t attr = event->notify_rx.attr_handle;
        uint16_t pkt_len = OS_MBUF_PKTLEN(event->notify_rx.om);
        uint8_t* report_data = OS_MBUF_DATA(event->notify_rx.om, uint8_t*);

        ESP_LOGI(TAG, "Notify RX: handle=%d len=%d", attr, pkt_len);
        if (pkt_len >= 8) {
            ESP_LOGI(TAG, "  Report: %02x %02x %02x %02x %02x %02x %02x %02x",
                     report_data[0], report_data[1], report_data[2], report_data[3],
                     report_data[4], report_data[5], report_data[6], report_data[7]);
        }

        // Process any notification from the connected device as potential HID data
        if (pkt_len >= 3) {
            self->processHidReport(report_data, pkt_len);
        }
        break;
    }

    default:
        break;
    }

    return 0;
}

} // namespace ble_hid
