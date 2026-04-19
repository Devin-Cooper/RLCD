#include "ble_hid.hpp"
#include "hid_report_process.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

// NimBLE includes
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "host/ble_sm.h"
#include "store/config/ble_store_config.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

// ble_store_config_init has no public prototype in ESP-IDF's NimBLE
// headers — the implementation lives in ble_store_config.c. Add a local
// forward declaration to match the canonical ESP-IDF example pattern
// (see esp-idf/examples/bluetooth/nimble/ble_cts/cts_cent/main/main.c).
extern "C" { void ble_store_config_init(void); }

#include <cstring>

static const char* TAG = "ble_hid";

// HID-over-GATT UUIDs (Bluetooth SIG assigned).
static const ble_uuid16_t HID_SVC_UUID         = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t HID_REPORT_UUID      = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t REPORT_REFERENCE_UUID = BLE_UUID16_INIT(0x2908);
static const ble_uuid16_t CCCD_UUID            = BLE_UUID16_INIT(0x2902);
static const ble_uuid16_t PROTOCOL_MODE_UUID   = BLE_UUID16_INIT(0x2A4E);
static const ble_uuid16_t HID_CONTROL_POINT_UUID = BLE_UUID16_INIT(0x2A4C);

namespace ble_hid {

BleHidHost* BleHidHost::s_instance_ = nullptr;

BleHidHost::BleHidHost()
    : state_(State::Disabled), key_cb_(nullptr), key_ctx_(nullptr),
      state_cb_(nullptr), state_ctx_(nullptr),
      conn_handle_(kInvalidHandle),
      reports_count_(0), subscribe_next_idx_(0), pending_ref_reads_(0),
      protocol_mode_handle_(kInvalidHandle),
      control_point_handle_(kInvalidHandle),
      hid_svc_start_handle_(0), hid_svc_end_handle_(0),
      pairing_timer_(nullptr),
      last_addr_type_(0), own_addr_type_(0) {
    std::memset(prev_keys_, 0, sizeof(prev_keys_));
    s_instance_ = this;
}

BleHidHost::~BleHidHost() {
    if (pairing_timer_) {
        esp_timer_stop(pairing_timer_);
        esp_timer_delete(pairing_timer_);
    }
    if (s_instance_ == this) s_instance_ = nullptr;
}

void BleHidHost::init() {
    // Initialize NimBLE host stack
    ESP_ERROR_CHECK(nimble_port_init());

    // Configure NimBLE host: bonding, secure connections, encryption + ID keys.
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sync_cb           = &BleHidHost::onSyncStatic;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    // Persist bonds across reboots (requires CONFIG_BT_NIMBLE_NVS_PERSIST=y).
    // Pure GATT client — local GAP/GATT service tables are not required.
    ble_store_config_init();

    // Start NimBLE host task; sync_cb fires once the controller reports ready.
    nimble_port_freertos_init([](void*) {
        nimble_port_run();
        nimble_port_freertos_deinit();
    });

    setState(State::Disconnected);
    ESP_LOGI(TAG, "BLE HID Host initialized (NimBLE started; awaiting sync)");
}

// Fires on the NimBLE host task once the controller is ready.
void BleHidHost::onSyncStatic() {
    auto* self = s_instance_;
    if (!self) return;

    // Ensure we have an identity address; infer the type NimBLE should use.
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &self->own_addr_type_);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE host sync complete (own_addr_type=%d)", self->own_addr_type_);
    self->autoReconnect();
}

void BleHidHost::startScan() {
    struct ble_gap_disc_params disc_params = {};
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    int rc = ble_gap_disc(own_addr_type_, BLE_HS_FOREVER,
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
    if (state_.load(std::memory_order_acquire) == State::Scanning) {
        setState(State::Disconnected);
    }
}

void BleHidHost::connect(const uint8_t addr[6]) {
    connectWithType(addr, last_addr_type_);
}

void BleHidHost::connectWithType(const uint8_t addr[6], uint8_t addr_type) {
    ble_addr_t peer_addr;
    peer_addr.type = addr_type;
    std::memcpy(peer_addr.val, addr, 6);

    stopScan();

    int rc = ble_gap_connect(own_addr_type_, &peer_addr,
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

    if (!pairing_timer_) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = pairingTimeoutCb;
        timer_args.arg = this;
        timer_args.name = "ble_pair_timeout";
        esp_timer_create(&timer_args, &pairing_timer_);
    }
    esp_timer_start_once(pairing_timer_,
                          static_cast<uint64_t>(timeout_sec) * 1000000ULL);
}

void BleHidHost::disconnect() {
    if (conn_handle_ != kInvalidHandle) {
        ble_gap_terminate(conn_handle_, BLE_ERR_REM_USER_CONN_TERM);
    }
    clearReportSlots();
    setState(State::Disconnected);
}

void BleHidHost::autoReconnect() {
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
            // Use the bonded peer's address type directly.
            last_addr_type_ = peer_addrs[0].type;
            connectWithType(peer_addrs[0].val, peer_addrs[0].type);
            return;
        }
    }
    ESP_LOGI(TAG, "No bonded peers, waiting for explicit pairing");
}

void BleHidHost::clearReportSlots() {
    reports_count_ = 0;
    subscribe_next_idx_ = 0;
    pending_ref_reads_ = 0;
    protocol_mode_handle_ = kInvalidHandle;
    control_point_handle_ = kInvalidHandle;
    hid_svc_start_handle_ = 0;
    hid_svc_end_handle_ = 0;
    for (int i = 0; i < kMaxReportSlots; ++i) {
        reports_[i] = ReportSlot{};
    }
    std::memset(prev_keys_, 0, sizeof(prev_keys_));
}

// --- HID Report Processing ---

void BleHidHost::processHidReport(const uint8_t* report, size_t len, uint8_t report_id) {
    // If report is prefixed with a non-zero report ID byte, strip it.
    const uint8_t* body = report;
    size_t body_len = len;
    if (report_id != 0 && len > 0 && report[0] == report_id) {
        body++;
        body_len--;
    }
    ble_hid::processHidReport(body, body_len, prev_keys_,
        [](const KeyEvent& ev, void* ctx) {
            auto* self = static_cast<BleHidHost*>(ctx);
            if (self->key_cb_) self->key_cb_(ev, self->key_ctx_);
        },
        this);
}

KeyEvent BleHidHost::translateKeycode(uint8_t keycode, uint8_t modifiers) {
    return ble_hid::translateKeycode(keycode, modifiers);
}

// --- State ---

void BleHidHost::setState(State s) {
    state_.store(s, std::memory_order_release);
    if (state_cb_) {
        state_cb_(s, state_ctx_);
    }
}

void BleHidHost::pairingTimeoutCb(void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);
    if (self->state_.load(std::memory_order_acquire) == State::Scanning) {
        ESP_LOGW(TAG, "Pairing timeout — exiting pairing mode");
        self->stopScan();
    }
}

// --- GATT Discovery Callbacks ---

// Step 1: service discovery — find HID service (0x1812).
int BleHidHost::gattSvcDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                               const struct ble_gatt_svc* service, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Service discovery complete");
        // If we never found an HID service, nothing to do.
        if (self->hid_svc_start_handle_ == 0) {
            ESP_LOGW(TAG, "Peer has no HID service");
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Service discovery error: %d", error->status);
        return 0;
    }

    if (ble_uuid_cmp(&service->uuid.u, &HID_SVC_UUID.u) == 0) {
        ESP_LOGI(TAG, "Found HID service (handles %d..%d)",
                 service->start_handle, service->end_handle);
        self->hid_svc_start_handle_ = service->start_handle;
        self->hid_svc_end_handle_   = service->end_handle;
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

// Step 2: characteristic discovery within HID service.
int BleHidHost::gattChrDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                               const struct ble_gatt_chr* chr, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Characteristic discovery complete (%d report slots)",
                 self->reports_count_);

        // Start descriptor discovery over the entire HID service range.
        // The descriptor callback will populate CCCD handles + Report
        // Reference descriptors and (via reference-read completion) kick
        // off CCCD subscription + Protocol Mode / Control Point writes.
        if (self->hid_svc_start_handle_ != 0) {
            int rc = ble_gattc_disc_all_dscs(conn_handle,
                                              self->hid_svc_start_handle_,
                                              self->hid_svc_end_handle_,
                                              gattDscDiscCb, self);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to start descriptor discovery: %d", rc);
            }
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Characteristic discovery error: %d", error->status);
        return 0;
    }

    // Report characteristic (0x2A4D): collect Input (Notify-capable) into a slot.
    if (ble_uuid_cmp(&chr->uuid.u, &HID_REPORT_UUID.u) == 0) {
        if (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) {
            if (self->reports_count_ < kMaxReportSlots) {
                self->reports_[self->reports_count_].val_handle = chr->val_handle;
                ESP_LOGI(TAG, "Report slot[%d]: val_handle=%d (props=0x%02x)",
                         self->reports_count_, chr->val_handle, chr->properties);
                self->reports_count_++;
            } else {
                ESP_LOGW(TAG, "kMaxReportSlots exceeded — dropping extra Report chr");
            }
        }
    } else if (ble_uuid_cmp(&chr->uuid.u, &PROTOCOL_MODE_UUID.u) == 0) {
        self->protocol_mode_handle_ = chr->val_handle;
        ESP_LOGI(TAG, "HID Protocol Mode handle: %d", chr->val_handle);
    } else if (ble_uuid_cmp(&chr->uuid.u, &HID_CONTROL_POINT_UUID.u) == 0) {
        self->control_point_handle_ = chr->val_handle;
        ESP_LOGI(TAG, "HID Control Point handle: %d", chr->val_handle);
    }
    return 0;
}

// Step 3: descriptor discovery across the whole HID service. We are looking
// for 0x2902 (CCCD) and 0x2908 (Report Reference). CCCD handles are
// associated with the nearest preceding Report characteristic in handle
// order; Report Reference values must be read asynchronously.
int BleHidHost::gattDscDiscCb(uint16_t conn_handle, const struct ble_gatt_error* error,
                               uint16_t chr_val_handle, const struct ble_gatt_dsc* dsc,
                               void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == BLE_HS_EDONE) {
        // Descriptor discovery complete. If there are no outstanding
        // reference reads, proceed to subscribe + Protocol Mode /
        // Control Point immediately; otherwise the last read's callback
        // will drive the continuation.
        if (self->pending_ref_reads_ == 0) {
            self->onAllReferenceReadsComplete();
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Descriptor discovery error: %d", error->status);
        return 0;
    }

    // chr_val_handle is the value handle of the characteristic that
    // precedes this descriptor in handle order. Find the matching slot.
    int slot_idx = -1;
    for (int i = 0; i < self->reports_count_; ++i) {
        if (self->reports_[i].val_handle == chr_val_handle) {
            slot_idx = i;
            break;
        }
    }

    if (ble_uuid_cmp(&dsc->uuid.u, &CCCD_UUID.u) == 0) {
        if (slot_idx >= 0) {
            self->reports_[slot_idx].cccd_handle = dsc->handle;
            ESP_LOGI(TAG, "Slot[%d] CCCD handle=%d", slot_idx, dsc->handle);
        }
    } else if (ble_uuid_cmp(&dsc->uuid.u, &REPORT_REFERENCE_UUID.u) == 0) {
        if (slot_idx >= 0) {
            // Read the Report Reference value asynchronously — payload is
            // 2 bytes: [report_id, report_type] where type 1=Input.
            self->pending_ref_reads_++;
            int rc = ble_gattc_read(conn_handle, dsc->handle,
                                     reportReferenceReadCb, self);
            if (rc != 0) {
                ESP_LOGE(TAG, "Slot[%d] Report Reference read failed: %d",
                         slot_idx, rc);
                self->pending_ref_reads_--;
            }
        }
    }
    return 0;
}

int BleHidHost::reportReferenceReadCb(uint16_t /*conn_handle*/,
                                      const struct ble_gatt_error* error,
                                      struct ble_gatt_attr* attr, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    if (error->status == 0 && attr && attr->om) {
        uint16_t len = OS_MBUF_PKTLEN(attr->om);
        if (len >= 2) {
            uint8_t body[2] = {};
            ble_hs_mbuf_to_flat(attr->om, body, sizeof(body), nullptr);
            uint8_t report_id   = body[0];
            uint8_t report_type = body[1];

            // The attribute handle of this descriptor is attr->handle — we
            // need the slot whose CCCD or val_handle is nearest below it.
            // Easiest: match by the Report-Reference's attr handle falling
            // between a slot's val_handle and the next slot's val_handle.
            // Since the descriptor discovery loop already paired CCCDs
            // with slots, we accept the first slot whose report_id is
            // still unset.
            for (int i = 0; i < self->reports_count_; ++i) {
                if (self->reports_[i].type == 0 && self->reports_[i].report_id == 0) {
                    self->reports_[i].report_id = report_id;
                    self->reports_[i].type      = report_type;
                    ESP_LOGI(TAG, "Slot[%d] Report Reference: id=%d type=%d",
                             i, report_id, report_type);
                    break;
                }
            }
        }
    } else {
        ESP_LOGW(TAG, "Report Reference read err=%d",
                 error ? error->status : -1);
    }

    self->pending_ref_reads_--;
    if (self->pending_ref_reads_ == 0) {
        self->onAllReferenceReadsComplete();
    }
    return 0;
}

void BleHidHost::onAllReferenceReadsComplete() {
    ESP_LOGI(TAG, "Descriptor discovery + Report Reference reads complete");

    // Write Protocol Mode = Report (0x01) and Control Point = Exit Suspend (0x01)
    // per HOGP §2.6 Table 3.4. Control Point: 0x00 = Suspend, 0x01 = Exit Suspend.
    writeProtocolMode(0x01);
    writeControlPoint(0x01);

    // Start subscribing to Input Reports.
    subscribe_next_idx_ = 0;
    subscribeNextInputReport();
}

void BleHidHost::subscribeNextInputReport() {
    // Advance through report slots, subscribing to any that look like
    // Input reports (type == 1, or type == 0 for boot-only devices
    // without a Report Reference descriptor).
    while (subscribe_next_idx_ < reports_count_) {
        ReportSlot& slot = reports_[subscribe_next_idx_];
        subscribe_next_idx_++;
        bool is_input = (slot.type == 0 || slot.type == 1);
        if (!is_input || slot.cccd_handle == kInvalidHandle) continue;

        uint8_t notify_enable[] = {0x01, 0x00};
        int rc = ble_gattc_write_flat(conn_handle_, slot.cccd_handle,
                                       notify_enable, sizeof(notify_enable),
                                       nullptr, nullptr);
        if (rc == 0) {
            ESP_LOGI(TAG, "Subscribed to Report slot val=%d cccd=%d id=%d type=%d",
                     slot.val_handle, slot.cccd_handle, slot.report_id, slot.type);
        } else {
            ESP_LOGE(TAG, "CCCD write failed (slot val=%d cccd=%d): %d",
                     slot.val_handle, slot.cccd_handle, rc);
        }
    }
    ESP_LOGI(TAG, "HID Input subscriptions complete");
}

void BleHidHost::writeProtocolMode(uint8_t value) {
    if (protocol_mode_handle_ == kInvalidHandle) {
        ESP_LOGD(TAG, "No Protocol Mode characteristic — skipping");
        return;
    }
    int rc = ble_gattc_write_no_rsp_flat(conn_handle_, protocol_mode_handle_,
                                          &value, 1);
    if (rc == 0) {
        ESP_LOGI(TAG, "Wrote Protocol Mode = %s (0x%02x)",
                 value == 0x01 ? "Report" : "Boot", value);
    } else {
        ESP_LOGE(TAG, "Protocol Mode write failed: %d", rc);
    }
}

void BleHidHost::writeControlPoint(uint8_t value) {
    if (control_point_handle_ == kInvalidHandle) {
        ESP_LOGD(TAG, "No HID Control Point characteristic — skipping");
        return;
    }
    // HOGP §2.6 Table 3.4: 0x00 = Suspend, 0x01 = Exit Suspend.
    int rc = ble_gattc_write_no_rsp_flat(conn_handle_, control_point_handle_,
                                          &value, 1);
    if (rc == 0) {
        ESP_LOGI(TAG, "Wrote HID Control Point = %s (0x%02x)",
                 value == 0x01 ? "Exit Suspend" : "Suspend", value);
    } else {
        ESP_LOGE(TAG, "Control Point write failed: %d", rc);
    }
}

// --- NimBLE GAP Event Handler ---

int BleHidHost::bleGapEvent(struct ble_gap_event* event, void* arg) {
    auto* self = static_cast<BleHidHost*>(arg);

    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
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

            if (self->pairing_timer_) {
                esp_timer_stop(self->pairing_timer_);
            }
            self->last_addr_type_ = event->disc.addr.type;
            self->connectWithType(event->disc.addr.val, event->disc.addr.type);
        }
        break;
    }

    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            self->conn_handle_ = event->connect.conn_handle;
            self->clearReportSlots();
            self->setState(State::Connected);
            ESP_LOGI(TAG, "Connected (handle=%d)", self->conn_handle_);

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
        self->conn_handle_ = kInvalidHandle;
        self->clearReportSlots();
        self->setState(State::Disconnected);
        ESP_LOGW(TAG, "Disconnected (reason=%d)", event->disconnect.reason);
        break;
    }

    case BLE_GAP_EVENT_ENC_CHANGE: {
        ESP_LOGI(TAG, "Encryption change: status=%d", event->enc_change.status);
        break;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // Delete the stale bond and let NimBLE retry.
        struct ble_gap_conn_desc desc;
        int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        ESP_LOGW(TAG, "Repeat pairing — deleted old bond, retrying");
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        // Default decline — RLCD has no passkey entry UI (out of scope).
        ESP_LOGW(TAG, "Passkey action requested (action=%d) — not supported",
                 event->passkey.params.action);
        struct ble_sm_io pkey = {};
        pkey.action = event->passkey.params.action;
        pkey.numcmp_accept = 0;
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        break;
    }

    case BLE_GAP_EVENT_MTU: {
        ESP_LOGD(TAG, "MTU updated: %d", event->mtu.value);
        break;
    }

    case BLE_GAP_EVENT_SUBSCRIBE: {
        ESP_LOGD(TAG, "Subscribe event: attr=%d reason=%d cur_notify=%d",
                 event->subscribe.attr_handle,
                 event->subscribe.reason, event->subscribe.cur_notify);
        break;
    }

    case BLE_GAP_EVENT_NOTIFY_RX: {
        uint16_t attr = event->notify_rx.attr_handle;
        uint16_t pkt_len = OS_MBUF_PKTLEN(event->notify_rx.om);
        uint8_t* report_data = OS_MBUF_DATA(event->notify_rx.om, uint8_t*);

        ESP_LOGD(TAG, "Notify RX: handle=%d len=%d", attr, pkt_len);

        // Route by handle to the matching slot so we know report_id + type.
        int slot_idx = -1;
        for (int i = 0; i < self->reports_count_; ++i) {
            if (self->reports_[i].val_handle == attr) {
                slot_idx = i;
                break;
            }
        }
        if (slot_idx < 0) {
            ESP_LOGD(TAG, "Notify from unsubscribed handle %d — ignoring", attr);
            break;
        }
        ReportSlot& slot = self->reports_[slot_idx];
        if (slot.type != 0 && slot.type != 1) {
            // Output / Feature report — not keyboard input.
            break;
        }
        self->processHidReport(report_data, pkt_len, slot.report_id);
        break;
    }

    default:
        break;
    }

    return 0;
}

} // namespace ble_hid
