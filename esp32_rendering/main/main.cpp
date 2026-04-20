#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_sntp.h>
#include <esp_littlefs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <atomic>
#include <memory>

// Display and sensors
#include "st7305.hpp"
#include "i2c_bsp.hpp"
#include "pcf85063.hpp"
#include "shtc3.hpp"
#include "battery.hpp"
#include "buttons.hpp"

// Connectivity
#include "wifi_manager.hpp"
#include "ssh_client.hpp"
#include "ble_hid.hpp"
#include "input_queue.hpp"

// Application layer
#include "settings.hpp"
#include "dashboard.hpp"
#include "terminal_mode.hpp"
#include "screen.hpp"
#include "screen_stack.hpp"
#include "screen_context.hpp"
#include "overlay.hpp"
#include "screens/dashboard_screen.hpp"
#include "screens/terminal_screen.hpp"
#include "screens/pairing_screen.hpp"
#include "screens/wifi_screen.hpp"

// SD card config
#include "sdcard_manager.hpp"
#include "config_manager.hpp"
#include "config_store_nvs.hpp"

// onebit library (via EXTRA_COMPONENT_DIRS)
#include <1bit/core/allocator.hpp>
#include <1bit/core/framebuffer.hpp>
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/fonts/term_5x7.hpp>
#include <1bit/fonts/term_6x9.hpp>
#include <1bit/fonts/term_8x12.hpp>

// Legacy rendering (clock face, shapes)
#include "rendering/framebuffer.hpp"

static const char* TAG = "main";

// ============================================================================
// SSH data stream buffer — SSH task writes, main loop reads
// ============================================================================
static StreamBufferHandle_t sshDataStream = nullptr;
static constexpr size_t SSH_STREAM_SIZE = 8192;

// ============================================================================
// WiFi connected atomic — bridged into the onStateChange lambda via a
// file-scope pointer so the captureless callback can update it.
// ============================================================================
static std::atomic<bool>* s_wifi_connected = nullptr;

// ============================================================================
// Framebuffer adapter: onebit::IFramebuffer <-> rendering::IFramebuffer
// Both use packed 1-bit rows (MSB-first), identical binary layout at 400x300.
// The ST7305 driver accepts rendering::IFramebuffer& — this thin wrapper
// lets us render with onebit:: types and display via the existing driver.
// ============================================================================

class FramebufferAdapter : public rendering::IFramebuffer {
public:
    explicit FramebufferAdapter(onebit::IFramebuffer& src) : src_(src) {}

    int16_t width() const override { return src_.width(); }
    int16_t height() const override { return src_.height(); }

    void setPixel(int16_t x, int16_t y, rendering::Color c) override {
        src_.setPixel(x, y, static_cast<onebit::Color>(c));
    }
    rendering::Color getPixel(int16_t x, int16_t y) const override {
        return static_cast<rendering::Color>(src_.getPixel(x, y));
    }
    void clear(rendering::Color c = rendering::WHITE) override {
        src_.clear(static_cast<onebit::Color>(c));
    }
    void setPixelDirect(int16_t x, int16_t y, rendering::Color c) override {
        src_.setPixelDirect(x, y, static_cast<onebit::Color>(c));
    }
    void fillSpan(int16_t y, int16_t xStart, int16_t xEnd, rendering::Color c) override {
        src_.fillSpan(y, xStart, xEnd, static_cast<onebit::Color>(c));
    }

    uint8_t* buffer() override { return src_.buffer(); }
    const uint8_t* buffer() const override { return src_.buffer(); }
    size_t bufferSize() const override { return src_.bufferSize(); }

private:
    onebit::IFramebuffer& src_;
};

// Forward declarations for helpers defined lower in this file.
static const onebit::BitmapFont& fontForSize(uint8_t size);
static void showStatus(onebit::IFramebuffer& fb, st7305::Display& display,
                       const char* line1, const char* line2 = nullptr);

// ============================================================================
// Server rotation — shared by DashboardScreen Btn B long (wired via
// ctx.switchToNextServer) and any future menu "Servers" confirm paths.
// ============================================================================

static void switchToNextServer(sdcard::ConfigManager* configMgr,
                                ssh::SshClient& sshClient,
                                app::Dashboard& dashboard) {
    if (configMgr->serverCount() <= 1) return;
    int next = (configMgr->activeServerIndex() + 1) % configMgr->serverCount();
    configMgr->setActiveServer(next);
    const auto& srv = configMgr->activeServer();
    sshClient.disconnect();
    ssh::Config cfg = {};
    strncpy(cfg.host, srv.creds.host, sizeof(cfg.host) - 1);
    cfg.port = srv.creds.port;
    strncpy(cfg.username, srv.creds.username, sizeof(cfg.username) - 1);
    cfg.use_key_auth = srv.creds.use_key_auth;
    if (!cfg.use_key_auth) {
        strncpy(cfg.password, srv.creds.password, sizeof(cfg.password) - 1);
    }
    sshClient.connect(cfg);
    if (srv.dashboard_count > 0) {
        dashboard.updateCommands(srv.dashboard, srv.dashboard_count);
    }
    dashboard.setServerName(srv.creds.name[0] ? srv.creds.name : srv.creds.host);
    ESP_LOGI(TAG, "Switched to server: %s", srv.creds.name);
}

// ============================================================================
// Reconnect to current active server index (no rotation).
// Used by ServerListScreen Shift+A (Task 21) and any "apply the edit"
// flow that changes the active server.
// ============================================================================

static void reconnectActiveServer(sdcard::ConfigManager* configMgr,
                                   ssh::SshClient& sshClient,
                                   app::Dashboard& dashboard) {
    if (configMgr->serverCount() == 0) return;
    const auto& srv = configMgr->activeServer();
    sshClient.disconnect();
    ssh::Config cfg = {};
    strncpy(cfg.host, srv.creds.host, sizeof(cfg.host) - 1);
    cfg.port = srv.creds.port;
    strncpy(cfg.username, srv.creds.username, sizeof(cfg.username) - 1);
    cfg.use_key_auth = srv.creds.use_key_auth;
    if (!cfg.use_key_auth) {
        strncpy(cfg.password, srv.creds.password, sizeof(cfg.password) - 1);
    }
    sshClient.connect(cfg);
    if (srv.dashboard_count > 0) {
        dashboard.updateCommands(srv.dashboard, srv.dashboard_count);
    }
    dashboard.setServerName(srv.creds.name[0] ? srv.creds.name : srv.creds.host);
    ESP_LOGI(TAG, "Reconnected to active server: %s", srv.creds.name);
}

// ============================================================================
// Font table — indexed by Settings::font_size (0/1/2)
// ============================================================================

static const onebit::BitmapFont& fontForSize(uint8_t size) {
    switch (size) {
        case 0: return onebit::fonts::TERM_5X7;
        case 2: return onebit::fonts::TERM_8X12;
        default: return onebit::fonts::TERM_6X9;
    }
}

// ============================================================================
// Splash screen
// ============================================================================

static void showSplash(onebit::IFramebuffer& fb, st7305::Display& display) {
    fb.clear(onebit::WHITE);

    // Border
    onebit::drawRect(fb, 0, 0, fb.width(), fb.height(), onebit::BLACK);
    onebit::drawRect(fb, 2, 2, fb.width() - 4, fb.height() - 4, onebit::BLACK);

    // Title centered
    const auto& font = onebit::fonts::TERM_8X12;
    const char* title = "RLCD Terminal";
    int16_t tw = onebit::getBitmapTextWidth(font, title);
    onebit::drawBitmapText(fb, font,
                           (fb.width() - tw) / 2, fb.height() / 2 - 20,
                           title, onebit::BLACK);

    const char* sub = "Initializing...";
    int16_t sw = onebit::getBitmapTextWidth(font, sub);
    onebit::drawBitmapText(fb, font,
                           (fb.width() - sw) / 2, fb.height() / 2 + 4,
                           sub, onebit::BLACK);

    // Show via adapter
    FramebufferAdapter adapter(fb);
    display.show(adapter);
}

// ============================================================================
// Status / error screens
// ============================================================================

static void showStatus(onebit::IFramebuffer& fb, st7305::Display& display,
                       const char* line1, const char* line2) {
    fb.clear(onebit::WHITE);
    const auto& font = onebit::fonts::TERM_6X9;
    onebit::drawBitmapText(fb, font, 10, 10, line1, onebit::BLACK);
    if (line2) {
        onebit::drawBitmapText(fb, font, 10, 24, line2, onebit::BLACK);
    }
    FramebufferAdapter adapter(fb);
    display.show(adapter);
}

// ============================================================================
// NVS initialization (with encryption fallback)
// ============================================================================

static bool initNvs() {
    // Try NVS with encryption first (requires eFuse-provisioned key partition)
    esp_err_t err = nvs_flash_secure_init_partition("nvs", nullptr);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
        err == ESP_ERR_NOT_FOUND) {
        // Secure init failed — fall back to regular init (encryption not provisioned yet)
        ESP_LOGW(TAG, "NVS encryption not available, using unencrypted NVS");
        err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGW(TAG, "NVS partition truncated, erasing...");
            nvs_flash_erase();
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// ============================================================================
// LittleFS initialization for SSH keys and config
// ============================================================================

static bool initLittleFs() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(ret));
        return false;
    }
    // Create directories for SSH keys and config
    mkdir("/littlefs/known_hosts", 0755);
    ESP_LOGI(TAG, "LittleFS mounted");
    return true;
}

// ============================================================================
// NTP time sync
// ============================================================================

static void startNtpSync() {
    ESP_LOGI(TAG, "Starting NTP sync");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

// ============================================================================
// Main entry point
// ============================================================================

extern "C" void app_main() {
    ESP_LOGI(TAG, "RLCD Terminal — boot sequence start");

    // ------------------------------------------------------------------
    // Step 0: Set up DMA-capable allocator for onebit framebuffers
    // ------------------------------------------------------------------
    // Framebuffers must be in DMA-capable internal SRAM for SPI display
    // transfers. TerminalBuffer (allocated via std::calloc) will still
    // route to PSRAM for large allocs, which is correct.
    static onebit::Allocator dma_allocator = {
        .alloc = [](size_t size, void*) -> void* {
            return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        },
        .free = [](void* ptr, void*) { heap_caps_free(ptr); },
        .ctx = nullptr,
    };
    onebit::init(dma_allocator);

    // ------------------------------------------------------------------
    // Step 1: Display init + splash
    // ------------------------------------------------------------------
    onebit::Framebuffer<400, 300> fb;
    if (!fb.buffer()) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        return;
    }

    st7305::Config displayConfig;
    st7305::Display display(displayConfig);
    display.init();

    showSplash(fb, display);
    vTaskDelay(pdMS_TO_TICKS(500));

    // ------------------------------------------------------------------
    // Step 2: NVS
    // ------------------------------------------------------------------
    showStatus(fb, display, "Initializing NVS...");
    if (!initNvs()) {
        showStatus(fb, display, "NVS init failed", "Check flash partition");
        return;
    }

    // ------------------------------------------------------------------
    // Step 3: LittleFS
    // ------------------------------------------------------------------
    showStatus(fb, display, "Mounting LittleFS...");
    initLittleFs();

    // ------------------------------------------------------------------
    // Step 3a: Physical buttons (early — before blocking network waits)
    // ------------------------------------------------------------------
    buttons::ButtonHandler btns;
    if (btns.init()) {
        btns.onEvent(buttons::Button::A, buttons::Event::SingleClick,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn A short");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonShort;
                ie.button_id = 0;
                input::globalInputQueue().pushOrDrop(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::A, buttons::Event::LongPressStart,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn A long");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonLong;
                ie.button_id = 0;
                input::globalInputQueue().pushOrDrop(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::B, buttons::Event::SingleClick,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn B short");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonShort;
                ie.button_id = 1;
                input::globalInputQueue().pushOrDrop(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::B, buttons::Event::LongPressStart,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn B long");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonLong;
                ie.button_id = 1;
                input::globalInputQueue().pushOrDrop(ie);
            }, nullptr);

        btns.startAutoUpdate();
    } else {
        ESP_LOGW(TAG, "Button init failed");
    }

    // ------------------------------------------------------------------
    // Step 3b: SD Card
    // ------------------------------------------------------------------
    showStatus(fb, display, "Checking SD card...");
    sdcard::SDCardManager sdcard;
    // ConfigManager has ~14KB of ServerConfig arrays — heap allocate (via
    // unique_ptr) to avoid stack overflow and clean up automatically.
    auto configMgr = std::make_unique<sdcard::ConfigManager>();
    bool hasServers = false;

    bool sdMounted = sdcard.mount();
    if (sdMounted) {
        showStatus(fb, display, "SD card found");
    } else {
        showStatus(fb, display, "No SD card", "Using stored config");
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    // ------------------------------------------------------------------
    // Load application settings
    // ------------------------------------------------------------------
    showStatus(fb, display, "Loading settings...");
    ESP_LOGI(TAG, "Loading settings from NVS");
    app::Settings settings = app::loadSettings();
    ESP_LOGI(TAG, "Settings loaded OK");
    if (sdMounted) {
        const auto& ps = configMgr->parsedSettings();
        if (ps.has_font_size) settings.font_size = ps.font_size;
        if (ps.has_scrollback) settings.scrollback_depth = ps.scrollback;
        if (ps.has_dashboard_interval_ms) settings.dashboard_interval_ms = ps.dashboard_interval_ms;
        (void) app::saveSettings(settings);   // boot-time; failure logged by saveSettings itself
    }
    const onebit::BitmapFont& activeFont = fontForSize(settings.font_size);
    ESP_LOGI(TAG, "Font selected OK");

    // ------------------------------------------------------------------
    // Step 4: BLE HID
    // ------------------------------------------------------------------
    showStatus(fb, display, "Starting Bluetooth...");
    ESP_LOGI(TAG, "About to init BLE HID");
    ble_hid::BleHidHost bleHost;
    ESP_LOGI(TAG, "BleHidHost constructed");
    bleHost.init();
    ESP_LOGI(TAG, "BLE init returned");

    // BLE state change callback — log and post synthetic BleStateChanged
    // event so PairingScreen (and any future listeners) can react
    // single-threaded via Screen::handleInput. Must stay BEFORE autoReconnect.
    bleHost.onStateChange([](ble_hid::State state, void*) {
        const char* names[] = {"Disabled", "Scanning", "Connecting",
                               "Connected", "Disconnected"};
        int idx = static_cast<int>(state);
        ESP_LOGI("ble_hid", "State changed: %s (%d)",
                 (idx >= 0 && idx <= 4) ? names[idx] : "unknown", idx);
        // Post synthetic event so PairingScreen (and any future listeners)
        // can react single-threaded via Screen::handleInput.
        input::InputEvent ie{};
        ie.source = input::Source::System;
        ie.type   = input::EventType::BleStateChanged;
        ie.data[0] = static_cast<uint8_t>(state);
        ie.data_length = 1;
        input::globalInputQueue().pushOrDrop(ie);
    }, nullptr);

    // Bridge BLE key events into the unified input queue
    bleHost.onKey([](const ble_hid::KeyEvent& evt, void*) {
        input::InputEvent ie{};
        ie.source = input::Source::Keyboard;
        ie.type = input::EventType::Keypress;
        ie.data_length = evt.length;
        memcpy(ie.data, evt.bytes,
               evt.length < sizeof(ie.data) ? evt.length : sizeof(ie.data));
        bool ok = input::globalInputQueue().pushOrDrop(ie);
        ESP_LOGI("main", "[bridge] BLE key -> queue len=%d first=%02x pushed=%d",
                 evt.length, evt.length > 0 ? evt.bytes[0] : 0, ok ? 1 : 0);
    }, nullptr);

    bleHost.autoReconnect();
    ESP_LOGI(TAG, "BLE HID: auto-reconnect started");

    // ------------------------------------------------------------------
    // Step 5: WiFi
    // ------------------------------------------------------------------
    wifi::WifiManager wifiMgr;
    wifiMgr.init();

    // Debug: list files on SD card
    if (sdMounted) {
        ESP_LOGI(TAG, "=== SD card file listing ===");
        DIR* d = opendir("/sdcard");
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != nullptr) {
                ESP_LOGI(TAG, "  /sdcard/%s (type=%d)", e->d_name, e->d_type);
            }
            closedir(d);
        } else {
            ESP_LOGE(TAG, "Failed to open /sdcard directory");
        }
        DIR* d2 = opendir("/sdcard/servers");
        if (d2) {
            struct dirent* e;
            while ((e = readdir(d2)) != nullptr) {
                ESP_LOGI(TAG, "  /sdcard/servers/%s", e->d_name);
            }
            closedir(d2);
        } else {
            ESP_LOGW(TAG, "No /sdcard/servers/ directory");
        }
        ESP_LOGI(TAG, "=== end file listing ===");
    }

    // SD card config import (after WiFi init so saveNetwork works)
    if (sdMounted) {
        showStatus(fb, display, "Importing SD config...");
        int count = configMgr->init(wifiMgr);
        hasServers = (count > 0);
        ESP_LOGI(TAG, "SD card: %d server(s) loaded", count);

        char msg[64];
        snprintf(msg, sizeof(msg), "%d server(s) loaded", count);
        showStatus(fb, display, "SD import done", msg);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        ESP_LOGI(TAG, "No SD card — using stored config");
    }

    std::atomic<bool> wifiConnected{false};

    // WiFi state change callback — updates wifiConnected atomic AND posts
    // a synthetic WifiStateChanged event so screens react single-threaded.
    // Must stay BEFORE wifiMgr.autoConnect() so the first transition is
    // not lost. Amendment C.
    s_wifi_connected = &wifiConnected;
    wifiMgr.onStateChange([](wifi::State state, void*) {
        if (s_wifi_connected)
            s_wifi_connected->store(state == wifi::State::Connected);
        input::InputEvent ie{};
        ie.source = input::Source::System;
        ie.type   = input::EventType::WifiStateChanged;
        ie.data[0] = static_cast<uint8_t>(state);
        ie.data[1] = 0;   // reason — threaded in Task 16/Amendment L
        ie.data_length = 2;
        input::globalInputQueue().pushOrDrop(ie);
    }, nullptr);

    // Debug: dump stored WiFi credentials
    {
        nvs_handle_t h;
        if (nvs_open("wifi_creds", NVS_READONLY, &h) == ESP_OK) {
            for (int i = 0; i < 8; i++) {
                char ks[16], kp[16];
                char ssid[33] = {}, pass[65] = {};
                snprintf(ks, sizeof(ks), "ssid_%d", i);
                snprintf(kp, sizeof(kp), "pass_%d", i);
                size_t sl = sizeof(ssid), pl = sizeof(pass);
                if (nvs_get_str(h, ks, ssid, &sl) == ESP_OK) {
                    nvs_get_str(h, kp, pass, &pl);
                    ESP_LOGI(TAG, "NVS WiFi[%d]: SSID='%s' pass_len=%d", i, ssid, (int)strlen(pass));
                    if (i == 0) {
                        char dbg[80];
                        snprintf(dbg, sizeof(dbg), "SSID: %s (%d)", ssid, (int)strlen(pass));
                        showStatus(fb, display, "WiFi from NVS:", dbg);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                }
            }
            nvs_close(h);
        } else {
            ESP_LOGW(TAG, "No wifi_creds NVS namespace found");
            showStatus(fb, display, "No WiFi credentials", "Check SD card config");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    wifiMgr.autoConnect();

    // Wait up to 10 seconds for WiFi
    showStatus(fb, display, "Connecting to WiFi...");
    for (int i = 0; i < 100 && !wifiConnected; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // ------------------------------------------------------------------
    // Step 6/7: Post-WiFi routing
    // ------------------------------------------------------------------
    ssh::SshClient sshClient;
    std::atomic<bool> sshConnected{false};

    // Create SSH data stream buffer for SSH task -> main loop data flow
    sshDataStream = xStreamBufferCreate(SSH_STREAM_SIZE, 1);

    ssh::Config sshCfg{};
    if (wifiConnected) {
        ESP_LOGI(TAG, "WiFi connected");

        // NTP sync
        startNtpSync();

        // SSH connect — use SD card server config if available, else NVS settings
        if (hasServers) {
            const auto& srv = configMgr->activeServer();
            strncpy(sshCfg.host, srv.creds.host, sizeof(sshCfg.host) - 1);
            sshCfg.port = srv.creds.port;
            strncpy(sshCfg.username, srv.creds.username, sizeof(sshCfg.username) - 1);
            sshCfg.use_key_auth = srv.creds.use_key_auth;
            // Password is now stored directly in ServerCreds (Task 20)
            if (!sshCfg.use_key_auth) {
                strncpy(sshCfg.password, srv.creds.password, sizeof(sshCfg.password) - 1);
            }
        } else {
            strncpy(sshCfg.host, settings.ssh_host, sizeof(sshCfg.host) - 1);
            sshCfg.port = settings.ssh_port;
            strncpy(sshCfg.username, settings.ssh_user, sizeof(sshCfg.username) - 1);
            sshCfg.use_key_auth = (settings.auth_method == 1);
        }

        sshClient.onStateChange([](ssh::State state, const char* msg, void* ctx) {
            auto* flag = static_cast<std::atomic<bool>*>(ctx);
            if (state == ssh::State::Connected) {
                flag->store(true);
                ESP_LOGI("ssh", "Connected");
            } else if (state == ssh::State::Error) {
                ESP_LOGE("ssh", "Error: %s", msg);
            }
        }, &sshConnected);

        // Only attempt SSH if host is configured
        if (sshCfg.host[0] != '\0') {
            showStatus(fb, display, "Connecting to SSH...", sshCfg.host);
            sshClient.connect(sshCfg);

            // Wait up to 15 seconds for SSH
            for (int i = 0; i < 150 && !sshConnected; ++i) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            if (!sshConnected) {
                // Step 8: SSH failed — overlay will show error after stack is live
            } else {
                ESP_LOGI(TAG, "SSH connected");
            }
        } else {
            ESP_LOGW(TAG, "No SSH host configured — entering dashboard offline");
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected — user must configure via menu");
    }

    // ------------------------------------------------------------------
    // Initialize application-layer components
    // ------------------------------------------------------------------
    app::Dashboard dashboard;
    dashboard.init(settings);

    // Load dashboard commands and server name from active server config
    if (hasServers) {
        const auto& srv = configMgr->activeServer();
        if (srv.dashboard_count > 0) {
            dashboard.updateCommands(srv.dashboard, srv.dashboard_count);
        }
        // Show server name (or host) in dashboard title bar
        dashboard.setServerName(srv.creds.name[0] ? srv.creds.name : srv.creds.host);
    }

    app::TerminalMode terminalMode(fb, activeFont);
    uint8_t currentFontSize = settings.font_size;

    // ------------------------------------------------------------------
    // ScreenStack + OverlayManager + ScreenContext
    // ------------------------------------------------------------------
    app::ScreenStack stack;
    app::OverlayManager overlay;

    app::ScreenContext ctx{
        fb, display, sshClient, wifiMgr, *configMgr, bleHost, settings,
        stack, overlay, dashboard, terminalMode, currentFontSize
    };

    // Amendment K: wire switchToNextServer and switchToActiveServer into ctx.
    ctx.switchToNextServer = [&]() {
        switchToNextServer(configMgr.get(), sshClient, dashboard);
    };
    ctx.switchToActiveServer = [&]() {
        reconnectActiveServer(configMgr.get(), sshClient, dashboard);
    };

    stack.push(std::make_unique<app::DashboardScreen>(ctx));

    // Post-stack-init: show deferred boot errors via overlay now that it's live.
    if (wifiConnected && sshCfg.host[0] != '\0' && !sshConnected) {
        overlay.showError("SSH failed",
                          sshCfg.host[0] ? sshCfg.host : "no host configured");
    }
    if (!wifiConnected) {
        ESP_LOGW(TAG, "WiFi not connected — pushing WifiScreen");
        stack.push(std::make_unique<app::WifiScreen>(ctx));
    }

    // Surface migration result via overlay (toasts / log / error modal).
    {
        using MR = sdcard::MigrationResult;
        switch (configMgr->lastMigration()) {
            case MR::PathB:
                overlay.showToast("Migrated legacy server", 2500);
                break;
            case MR::BeltAndSuspenders:
                overlay.showToast("Discarded legacy ssh_host from Settings", 3000);
                break;
            case MR::PathAHole:
                ESP_LOGW(TAG, "Legacy ssh_creds preserved for next-boot migration");
                break;
            default: break;
        }
        if (configMgr->invalidJsonCount() > 0) {
            char body[96];
            snprintf(body, sizeof(body), "%d server file(s) invalid",
                     configMgr->invalidJsonCount());
            overlay.showError("SD parse errors", body);
        }
    }

    // Wire SSH data into stream buffer — SSH task pushes, main loop drains
    sshClient.onData([](const uint8_t* data, size_t len, void* ctx) {
        (void)ctx;
        if (sshDataStream && len > 0) {
            xStreamBufferSend(sshDataStream, data, len, pdMS_TO_TICKS(10));
        }
    }, nullptr);

    // Terminal output callback → SSH send
    terminalMode.setOutputCallback(
        [&sshClient](const uint8_t* data, size_t len) {
            if (sshClient.state() == ssh::State::Connected) {
                sshClient.send(data, len);
            }
        }
    );

    // Terminal resize → SSH resize
    terminalMode.setResizeCallback(
        [&sshClient](int cols, int rows) {
            if (sshClient.state() == ssh::State::Connected) {
                sshClient.resizeTerminal(cols, rows);
            }
        }
    );

    // ------------------------------------------------------------------
    // Adapter for ST7305 display (expects rendering::IFramebuffer)
    // ------------------------------------------------------------------
    FramebufferAdapter displayAdapter(fb);

    // Previous frame for dirty-region tracking
    onebit::Framebuffer<400, 300> prevFb;
    FramebufferAdapter prevAdapter(prevFb);
    bool hasPrevFrame = prevFb.buffer() != nullptr;

    ESP_LOGI(TAG, "Entering main loop");

    // Drop-counter watchdog state (Spec 05): rate-limit the warning so
    // sustained backpressure doesn't flood the log.
    uint32_t lastDroppedCount = 0;
    int64_t  lastDropLogUs = 0;
    constexpr int64_t DROP_LOG_INTERVAL_US = 1000000;  // 1 Hz

    // ==================================================================
    // Main loop
    // ==================================================================
    while (true) {
        int64_t frameStart = esp_timer_get_time();
        int64_t now_ms = frameStart / 1000;

        overlay.tick(frameStart);

        // Log pushOrDrop drops when count advances (at most 1 Hz).
        {
            uint32_t cur = input::globalInputQueue().droppedCount();
            if (cur != lastDroppedCount &&
                (frameStart - lastDropLogUs) >= DROP_LOG_INTERVAL_US) {
                ESP_LOGW("input", "dropped %u events (cumulative)", cur);
                lastDroppedCount = cur;
                lastDropLogUs = frameStart;
            }
        }

        // ----------------------------------------------------------
        // Process input events — fully stack-driven (Task 14).
        // ----------------------------------------------------------
        input::InputEvent evt;
        while (input::globalInputQueue().pop(evt)) {
            // Global shortcut: Btn A long → clear to base + push PairingScreen.
            // No-op if PairingScreen is already top (prevents re-entry).
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonLong &&
                evt.button_id == 0 &&
                dynamic_cast<app::PairingScreen*>(stack.top()) == nullptr) {
                stack.clearToBaseAndPush(std::make_unique<app::PairingScreen>(ctx));
                continue;
            }
            // Amendment D: System events (WiFi/BLE state) bypass the overlay so
            // modals never swallow backend state transitions.
            if (evt.source == input::Source::System) {
                stack.top()->handleInput(evt, stack);
                continue;
            }
            if (!overlay.handleInput(evt)) {
                stack.top()->handleInput(evt, stack);
            }
        }
        stack.applyPending();

        // ----------------------------------------------------------
        // SSH data drain — route to the active primary-view screen.
        // Walk the stack for the active primary screen, skipping
        // transparent overlays like MenuScreen.
        // ----------------------------------------------------------
        if (sshDataStream) {
            uint8_t sshBuf[1024];
            size_t received = xStreamBufferReceive(sshDataStream, sshBuf,
                                                   sizeof(sshBuf), 0);
            if (received > 0) {
                app::DashboardScreen* ds = nullptr;
                app::TerminalScreen*  ts = nullptr;
                for (size_t i = stack.depth(); i-- > 0; ) {
                    app::Screen* s = stack.at(i);
                    if (!ds) ds = dynamic_cast<app::DashboardScreen*>(s);
                    if (!ts) ts = dynamic_cast<app::TerminalScreen*>(s);
                    if (ds || ts) break;
                }
                if (ts) ts->feedSshData(sshBuf, received);
                else if (ds) ds->feedSshData(sshBuf, received);
            }
        }

        // ----------------------------------------------------------
        // SSH disconnect detection — show toast and offer reconnect
        // ----------------------------------------------------------
        if (sshConnected.load() &&
            sshClient.state() != ssh::State::Connected &&
            sshClient.state() != ssh::State::Connecting &&
            sshClient.state() != ssh::State::Authenticating) {
            sshConnected.store(false);
            overlay.showToast("SSH disconnected — press B to reconnect", 3000);
        }

        // ----------------------------------------------------------
        // Per-frame update for Dashboard if it's on the stack.
        // ----------------------------------------------------------
        {
            app::DashboardScreen* ds = nullptr;
            for (size_t i = stack.depth(); i-- > 0; ) {
                ds = dynamic_cast<app::DashboardScreen*>(stack.at(i));
                if (ds) break;
            }
            if (ds) ds->tickUpdate(now_ms);
        }

        // ----------------------------------------------------------
        // Render — fully stack-driven
        // ----------------------------------------------------------
        stack.renderAll(fb, fontForSize(currentFontSize));
        overlay.render(fb, fontForSize(currentFontSize));

        // ----------------------------------------------------------
        // Display update (dirty-region optimized)
        // ----------------------------------------------------------
        if (hasPrevFrame) {
            display.showIfDirty(displayAdapter, prevAdapter);
        } else {
            display.show(displayAdapter);
        }

        // ----------------------------------------------------------
        // Power management: DISABLED
        // Light sleep stops the SPI bus which blanks the ST7305 reflective
        // display. Since the display draws no power (reflective, no backlight),
        // light sleep saves minimal current but causes a blank screen.
        // TODO: Re-enable with proper SPI bus re-init after wake.
        // ----------------------------------------------------------

        // ----------------------------------------------------------
        // Frame pacing: driven by the top screen's targetFps()
        // ----------------------------------------------------------
        int64_t targetUs = 1000000 / stack.top()->targetFps();
        int64_t elapsed = esp_timer_get_time() - frameStart;
        int64_t sleepUs = targetUs - elapsed;
        if (sleepUs > 1000) {
            vTaskDelay(pdMS_TO_TICKS(sleepUs / 1000));
        }
    }
}
