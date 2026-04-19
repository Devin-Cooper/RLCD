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
#include "menu.hpp"
#include "dashboard.hpp"
#include "terminal_mode.hpp"

// SD card config
#include "sdcard_manager.hpp"
#include "config_manager.hpp"

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

// ============================================================================
// Application mode
// ============================================================================

enum class AppMode : uint8_t {
    Dashboard,
    Terminal,
    NetworkSelect,
    Error,
    Pairing,
};

// ============================================================================
// Server rotation (shared by Button-B-long menu confirm and Keyboard-Enter
// menu confirm paths — dedup per Spec 05).
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
    strncpy(cfg.host, srv.host, sizeof(cfg.host) - 1);
    cfg.port = srv.port;
    strncpy(cfg.username, srv.username, sizeof(cfg.username) - 1);
    cfg.use_key_auth = srv.use_key_auth;
    if (!cfg.use_key_auth) {
        nvs_handle_t handle;
        if (nvs_open("ssh_creds", NVS_READONLY, &handle) == ESP_OK) {
            char nvs_key[24];
            snprintf(nvs_key, sizeof(nvs_key), "srv_p_%d", next);
            size_t len = sizeof(cfg.password);
            nvs_get_str(handle, nvs_key, cfg.password, &len);
            nvs_close(handle);
        }
    }
    sshClient.connect(cfg);
    if (srv.dashboard_count > 0) {
        dashboard.updateCommands(srv.dashboard, srv.dashboard_count);
    }
    dashboard.setServerName(srv.name[0] ? srv.name : srv.host);
    ESP_LOGI(TAG, "Switched to server: %s", srv.name);
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
                       const char* line1, const char* line2 = nullptr) {
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
                input::globalInputQueue().push(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::A, buttons::Event::LongPressStart,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn A long");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonLong;
                ie.button_id = 0;
                input::globalInputQueue().push(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::B, buttons::Event::SingleClick,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn B short");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonShort;
                ie.button_id = 1;
                input::globalInputQueue().push(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::B, buttons::Event::LongPressStart,
            [](buttons::Button, buttons::Event, void*) {
                ESP_LOGI("main", "btn B long");
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonLong;
                ie.button_id = 1;
                input::globalInputQueue().push(ie);
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
    // ConfigManager has ~14KB of ServerConfig arrays — heap allocate to avoid stack overflow
    auto* configMgr = new sdcard::ConfigManager();
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
        app::saveSettings(settings);
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

    // BLE state change callback — log and detect pairing success
    bleHost.onStateChange([](ble_hid::State state, void*) {
        const char* names[] = {"Disabled", "Scanning", "Connecting", "Connected", "Disconnected"};
        int idx = static_cast<int>(state);
        ESP_LOGI("ble_hid", "State changed: %s (%d)",
                 (idx >= 0 && idx <= 4) ? names[idx] : "unknown", idx);
    }, nullptr);

    // Bridge BLE key events into the unified input queue
    bleHost.onKey([](const ble_hid::KeyEvent& evt, void*) {
        input::InputEvent ie{};
        ie.source = input::Source::Keyboard;
        ie.type = input::EventType::Keypress;
        ie.data_length = evt.length;
        memcpy(ie.data, evt.bytes,
               evt.length < sizeof(ie.data) ? evt.length : sizeof(ie.data));
        input::globalInputQueue().push(ie);
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
    wifiMgr.onStateChange([](wifi::State state, void* ctx) {
        auto* flag = static_cast<std::atomic<bool>*>(ctx);
        flag->store(state == wifi::State::Connected);
    }, &wifiConnected);

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
    AppMode currentMode = AppMode::Dashboard;
    ssh::SshClient sshClient;
    std::atomic<bool> sshConnected{false};

    // Create SSH data stream buffer for SSH task -> main loop data flow
    sshDataStream = xStreamBufferCreate(SSH_STREAM_SIZE, 1);

    if (wifiConnected) {
        ESP_LOGI(TAG, "WiFi connected");

        // NTP sync
        startNtpSync();

        // SSH connect — use SD card server config if available, else NVS settings
        ssh::Config sshCfg{};
        if (hasServers) {
            const auto& srv = configMgr->activeServer();
            strncpy(sshCfg.host, srv.host, sizeof(sshCfg.host) - 1);
            sshCfg.port = srv.port;
            strncpy(sshCfg.username, srv.username, sizeof(sshCfg.username) - 1);
            sshCfg.use_key_auth = srv.use_key_auth;
            // Load password from NVS if using password auth
            if (!sshCfg.use_key_auth) {
                nvs_handle_t handle;
                if (nvs_open("ssh_creds", NVS_READONLY, &handle) == ESP_OK) {
                    char nvs_key[24];
                    snprintf(nvs_key, sizeof(nvs_key), "srv_p_%d",
                             configMgr->activeServerIndex());
                    size_t len = sizeof(sshCfg.password);
                    nvs_get_str(handle, nvs_key, sshCfg.password, &len);
                    nvs_close(handle);
                }
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
                // Step 8: SSH failed — show error
                showStatus(fb, display, "SSH connection failed",
                           "Press A for settings, B to retry");
                currentMode = AppMode::Error;
            } else {
                currentMode = AppMode::Dashboard;
            }
        } else {
            ESP_LOGW(TAG, "No SSH host configured — entering dashboard offline");
            currentMode = AppMode::Dashboard;
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected — network select");
        showStatus(fb, display, "WiFi not connected",
                   "Press A to scan networks");
        currentMode = AppMode::NetworkSelect;
    }

    // ------------------------------------------------------------------
    // Initialize application-layer components
    // ------------------------------------------------------------------
    app::Menu menu;
    app::Dashboard dashboard;
    dashboard.init(settings);

    // Load dashboard commands and server name from active server config
    if (hasServers) {
        const auto& srv = configMgr->activeServer();
        if (srv.dashboard_count > 0) {
            dashboard.updateCommands(srv.dashboard, srv.dashboard_count);
        }
        // Show server name (or host) in dashboard title bar
        dashboard.setServerName(srv.name[0] ? srv.name : srv.host);
    }

    app::TerminalMode terminalMode(fb, activeFont);
    uint8_t currentFontSize = settings.font_size;

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

    ESP_LOGI(TAG, "Entering main loop — mode=%d", static_cast<int>(currentMode));

    // ==================================================================
    // Main loop
    // ==================================================================
    while (true) {
        int64_t frameStart = esp_timer_get_time();
        int64_t now_ms = frameStart / 1000;

        // ----------------------------------------------------------
        // Process input events
        // ----------------------------------------------------------
        input::InputEvent evt;
        while (input::globalInputQueue().pop(evt)) {
            // --- Button A short press: pure toggle open/close ---
            // Confirmation moved to Button-B-long (below) and Keyboard-Enter
            // so rapid taps don't accidentally commit Item::Dashboard.
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonShort &&
                evt.button_id == 0) {
                if (menu.isOpen()) {
                    ESP_LOGI(TAG, "menu close");
                    menu.close();
                } else {
                    ESP_LOGI(TAG, "menu open");
                    menu.open();
                }
            }

            // --- Button A long press: BLE pairing ---
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonLong &&
                evt.button_id == 0) {
                ESP_LOGI(TAG, "BLE pairing mode triggered");
                currentMode = AppMode::Pairing;
                bleHost.startPairing(30);
            }

            // --- Button B short press: font cycle (terminal) or menu nav ---
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonShort &&
                evt.button_id == 1) {
                if (menu.isOpen()) {
                    menu.moveDown();
                } else if (currentMode == AppMode::Terminal) {
                    // Cycle font: 0→1→2→0
                    currentFontSize = (currentFontSize + 1) % 3;
                    settings.font_size = currentFontSize;
                    terminalMode.setFont(fontForSize(currentFontSize));
                    app::saveSettings(settings);
                } else if (currentMode == AppMode::Error ||
                           currentMode == AppMode::NetworkSelect) {
                    // Retry WiFi / SSH
                    wifiMgr.autoConnect();
                    currentMode = AppMode::Dashboard;
                }
            }

            // --- Button B long press: confirm menu OR terminal→dashboard swap ---
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonLong &&
                evt.button_id == 1) {
                if (menu.isOpen()) {
                    app::Menu::Item sel = menu.confirm();
                    menu.close();
                    ESP_LOGI(TAG, "menu confirm (btn B long): %d", static_cast<int>(sel));
                    switch (sel) {
                        case app::Menu::Item::Dashboard:
                            currentMode = AppMode::Dashboard;
                            break;
                        case app::Menu::Item::Terminal:
                            currentMode = AppMode::Terminal;
                            break;
                        case app::Menu::Item::Servers:
                            switchToNextServer(configMgr, sshClient, dashboard);
                            break;
                        case app::Menu::Item::Settings:
                            // TODO: settings editor screen
                            break;
                        case app::Menu::Item::WiFi:
                            currentMode = AppMode::NetworkSelect;
                            break;
                        case app::Menu::Item::About:
                            showStatus(fb, display, "RLCD Terminal v1.0",
                                       "github.com/Devin-Cooper/RLCD");
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            break;
                        default:
                            break;
                    }
                } else if (currentMode == AppMode::Terminal) {
                    currentMode = AppMode::Dashboard;
                    ESP_LOGI(TAG, "Returned to dashboard");
                }
            }

            // --- Keyboard input in terminal mode → send to SSH ---
            if (evt.source == input::Source::Keyboard &&
                evt.type == input::EventType::Keypress) {
                if (menu.isOpen()) {
                    // Arrow keys for menu navigation
                    if (evt.data_length == 3 &&
                        evt.data[0] == 0x1B && evt.data[1] == '[') {
                        if (evt.data[2] == 'A') menu.moveUp();      // Up
                        if (evt.data[2] == 'B') menu.moveDown();    // Down
                        if (evt.data[2] == 'C' || evt.data[2] == 'D') {
                            // Left/Right — confirm or close
                        }
                    }
                    // Enter confirms
                    if (evt.data_length == 1 && evt.data[0] == '\r') {
                        app::Menu::Item sel = menu.confirm();
                        menu.close();
                        ESP_LOGI(TAG, "menu confirm (kbd): %d", static_cast<int>(sel));
                        switch (sel) {
                            case app::Menu::Item::Dashboard:
                                currentMode = AppMode::Dashboard;
                                break;
                            case app::Menu::Item::Terminal:
                                currentMode = AppMode::Terminal;
                                break;
                            case app::Menu::Item::Servers: {
                                if (configMgr->serverCount() > 1) {
                                    int next = (configMgr->activeServerIndex() + 1) % configMgr->serverCount();
                                    configMgr->setActiveServer(next);
                                    const auto& srv = configMgr->activeServer();
                                    sshClient.disconnect();
                                    ssh::Config cfg = {};
                                    strncpy(cfg.host, srv.host, sizeof(cfg.host) - 1);
                                    cfg.port = srv.port;
                                    strncpy(cfg.username, srv.username, sizeof(cfg.username) - 1);
                                    cfg.use_key_auth = srv.use_key_auth;
                                    if (!cfg.use_key_auth) {
                                        nvs_handle_t handle;
                                        if (nvs_open("ssh_creds", NVS_READONLY, &handle) == ESP_OK) {
                                            char nvs_key[24];
                                            snprintf(nvs_key, sizeof(nvs_key), "srv_p_%d", next);
                                            size_t len = sizeof(cfg.password);
                                            nvs_get_str(handle, nvs_key, cfg.password, &len);
                                            nvs_close(handle);
                                        }
                                    }
                                    sshClient.connect(cfg);
                                    if (srv.dashboard_count > 0) {
                                        dashboard.updateCommands(srv.dashboard, srv.dashboard_count);
                                    }
                                    dashboard.setServerName(srv.name[0] ? srv.name : srv.host);
                                    ESP_LOGI(TAG, "Switched to server: %s", srv.name);
                                }
                                break;
                            }
                            case app::Menu::Item::Settings:
                                // TODO: settings editor screen
                                break;
                            case app::Menu::Item::WiFi:
                                currentMode = AppMode::NetworkSelect;
                                break;
                            case app::Menu::Item::About:
                                showStatus(fb, display, "RLCD Terminal v1.0",
                                           "github.com/Devin-Cooper/RLCD");
                                vTaskDelay(pdMS_TO_TICKS(2000));
                                break;
                            default:
                                break;
                        }
                    }
                    // Escape closes menu
                    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
                        ESP_LOGI(TAG, "menu close (esc)");
                        menu.close();
                    }
                } else {
                    // Menu is closed. F1 (ESC O P) opens the menu when in
                    // Terminal or Dashboard — intercept before forwarding to
                    // SSH so F1 doesn't reach the remote shell.
                    bool isF1 = (evt.data_length == 3 &&
                                 evt.data[0] == 0x1B &&
                                 evt.data[1] == 'O' &&
                                 evt.data[2] == 'P');
                    if (isF1 && (currentMode == AppMode::Terminal ||
                                 currentMode == AppMode::Dashboard)) {
                        ESP_LOGI(TAG, "menu open (F1)");
                        menu.open();
                    } else if (currentMode == AppMode::Terminal &&
                               sshClient.state() == ssh::State::Connected) {
                        sshClient.send(evt.data, evt.data_length);
                    }
                }
            }
        }

        // ----------------------------------------------------------
        // Drain SSH data stream buffer and route to active mode
        // ----------------------------------------------------------
        if (sshDataStream) {
            uint8_t sshBuf[1024];
            size_t received = xStreamBufferReceive(sshDataStream, sshBuf,
                                                    sizeof(sshBuf), 0);
            if (received > 0) {
                if (currentMode == AppMode::Terminal) {
                    terminalMode.feedData(sshBuf, received);
                } else if (currentMode == AppMode::Dashboard) {
                    dashboard.feedData(sshBuf, received);
                }
            }
        }

        // ----------------------------------------------------------
        // SSH disconnect detection — show error and offer reconnect
        // ----------------------------------------------------------
        if (sshConnected.load() &&
            sshClient.state() != ssh::State::Connected &&
            sshClient.state() != ssh::State::Connecting &&
            sshClient.state() != ssh::State::Authenticating) {
            sshConnected.store(false);
            showStatus(fb, display, "SSH disconnected",
                       "Press B to reconnect");
            currentMode = AppMode::Error;
        }

        // ----------------------------------------------------------
        // Render active mode
        // ----------------------------------------------------------
        switch (currentMode) {
            case AppMode::Dashboard:
                dashboard.update(sshClient, now_ms);
                dashboard.render(fb, fontForSize(currentFontSize));
                break;

            case AppMode::Terminal:
                terminalMode.render();
                break;

            case AppMode::NetworkSelect:
                // Minimal: show current WiFi info and prompt
                {
                    fb.clear(onebit::WHITE);
                    const auto& f = fontForSize(currentFontSize);
                    wifi::ConnectionInfo ci = wifiMgr.connectionInfo();
                    char buf[64];
                    snprintf(buf, sizeof(buf), "WiFi: %s",
                             ci.state == wifi::State::Connected ? ci.ssid : "disconnected");
                    onebit::drawBitmapText(fb, f, 10, 10, buf, onebit::BLACK);
                    onebit::drawBitmapText(fb, f, 10, 26,
                                           "Press B to scan, A for menu",
                                           onebit::BLACK);
                }
                break;

            case AppMode::Error:
                // Static — already rendered, just wait for input
                break;

            case AppMode::Pairing:
                {
                    fb.clear(onebit::WHITE);
                    const auto& f = fontForSize(currentFontSize);
                    onebit::drawBitmapText(fb, f, 10, 10, "BLE Pairing Mode",
                                           onebit::BLACK);
                    onebit::drawBitmapText(fb, f, 10, 30,
                                           "Put keyboard in pairing mode",
                                           onebit::BLACK);

                    const char* state_str = "Scanning...";
                    auto ble_state = bleHost.state();
                    if (ble_state == ble_hid::State::Connecting)
                        state_str = "Connecting...";
                    else if (ble_state == ble_hid::State::Connected) {
                        state_str = "Connected!";
                        // Auto-return to dashboard after connection
                        currentMode = AppMode::Dashboard;
                        ESP_LOGI(TAG, "BLE keyboard connected — returning to dashboard");
                    } else if (ble_state == ble_hid::State::Disconnected) {
                        state_str = "Timeout — no keyboard found";
                        // Return to dashboard after timeout
                        currentMode = AppMode::Dashboard;
                    }
                    onebit::drawBitmapText(fb, f, 10, 50, state_str,
                                           onebit::BLACK);

                    char dbg[48];
                    snprintf(dbg, sizeof(dbg), "State: %d", static_cast<int>(ble_state));
                    onebit::drawBitmapText(fb, f, 10, 70, dbg, onebit::BLACK);
                }
                break;
        }

        // ----------------------------------------------------------
        // Menu overlay (rendered on top of current mode)
        // ----------------------------------------------------------
        if (menu.isOpen()) {
            menu.render(fb, fontForSize(currentFontSize));
        }

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
        // Frame pacing: ~10 FPS for dashboard, ~30 FPS for terminal
        // ----------------------------------------------------------
        int64_t targetUs = (currentMode == AppMode::Terminal) ? 33333 : 100000;
        int64_t elapsed = esp_timer_get_time() - frameStart;
        int64_t sleepUs = targetUs - elapsed;
        if (sleepUs > 1000) {
            vTaskDelay(pdMS_TO_TICKS(sleepUs / 1000));
        }
    }
}
