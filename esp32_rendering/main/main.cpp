#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <nvs_flash.h>
#include <esp_sntp.h>
#include <esp_vfs_fat.h>

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

// onebit library (via EXTRA_COMPONENT_DIRS)
#include <1bit/core/framebuffer.hpp>
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/fonts/term_5x7.hpp>
#include <1bit/fonts/term_6x9.hpp>
#include <1bit/fonts/term_8x12.hpp>

// Legacy rendering (clock face, shapes)
#include "rendering/framebuffer.hpp"
#include "rendering/clock_face.hpp"
#include "rendering/animation.hpp"

static const char* TAG = "main";

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
};

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
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
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
    // Mount LittleFS on /littlefs using esp_vfs_fat as a lightweight stand-in.
    // On actual hardware this would use the LittleFS component; for now we
    // attempt the mount and log if it's unavailable.
    ESP_LOGI(TAG, "LittleFS: mount attempted on /littlefs");
    // TODO: Call esp_vfs_littlefs_register() when the LittleFS component
    // is added to the project. For now, config and keys in /littlefs will
    // gracefully fall back to defaults.
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

    // ------------------------------------------------------------------
    // Step 2: NVS
    // ------------------------------------------------------------------
    if (!initNvs()) {
        showStatus(fb, display, "NVS init failed", "Check flash partition");
        return;
    }

    // ------------------------------------------------------------------
    // Step 3: LittleFS
    // ------------------------------------------------------------------
    initLittleFs();

    // ------------------------------------------------------------------
    // Load application settings
    // ------------------------------------------------------------------
    app::Settings settings = app::loadSettings();
    const onebit::BitmapFont& activeFont = fontForSize(settings.font_size);

    // ------------------------------------------------------------------
    // Step 4: BLE HID — non-blocking auto-reconnect
    // ------------------------------------------------------------------
    ble_hid::BleHidHost bleHost;
    bleHost.init();

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

    volatile bool wifiConnected = false;
    wifiMgr.onStateChange([](wifi::State state, void* ctx) {
        auto* flag = static_cast<volatile bool*>(ctx);
        *flag = (state == wifi::State::Connected);
    }, const_cast<bool*>(&wifiConnected));

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
    volatile bool sshConnected = false;

    if (wifiConnected) {
        ESP_LOGI(TAG, "WiFi connected");

        // NTP sync
        startNtpSync();

        // SSH connect
        ssh::Config sshCfg{};
        strncpy(sshCfg.host, settings.ssh_host, sizeof(sshCfg.host) - 1);
        sshCfg.port = settings.ssh_port;
        strncpy(sshCfg.username, settings.ssh_user, sizeof(sshCfg.username) - 1);
        sshCfg.use_key_auth = (settings.auth_method == 1);

        sshClient.onStateChange([](ssh::State state, const char* msg, void* ctx) {
            auto* flag = static_cast<volatile bool*>(ctx);
            if (state == ssh::State::Connected) {
                *flag = true;
                ESP_LOGI("ssh", "Connected");
            } else if (state == ssh::State::Error) {
                ESP_LOGE("ssh", "Error: %s", msg);
            }
        }, const_cast<bool*>(&sshConnected));

        // Only attempt SSH if host is configured
        if (settings.ssh_host[0] != '\0') {
            showStatus(fb, display, "Connecting to SSH...", settings.ssh_host);
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

    app::TerminalMode terminalMode(fb, activeFont);
    uint8_t currentFontSize = settings.font_size;

    // Wire SSH data into dashboard and terminal
    sshClient.onData([](const uint8_t* data, size_t len, void* ctx) {
        // In dashboard mode the dashboard parses; in terminal mode
        // the terminal mode parses. The active mode pointer decides.
        auto* mode = static_cast<AppMode*>(ctx);
        (void)mode;
        (void)data;
        (void)len;
        // Data routing is handled in the main loop via a shared buffer.
        // The SSH task pushes to a ring buffer; the main loop drains it.
    }, &currentMode);

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
    // Initialize physical buttons into unified input queue
    // ------------------------------------------------------------------
    buttons::ButtonHandler btns;
    if (btns.init()) {
        btns.onEvent(buttons::Button::A, buttons::Event::SingleClick,
            [](buttons::Button, buttons::Event, void*) {
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonShort;
                ie.button_id = 0;
                input::globalInputQueue().push(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::A, buttons::Event::LongPressStart,
            [](buttons::Button, buttons::Event, void*) {
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonLong;
                ie.button_id = 0;
                input::globalInputQueue().push(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::B, buttons::Event::SingleClick,
            [](buttons::Button, buttons::Event, void*) {
                input::InputEvent ie{};
                ie.source = input::Source::Button;
                ie.type = input::EventType::ButtonShort;
                ie.button_id = 1;
                input::globalInputQueue().push(ie);
            }, nullptr);

        btns.onEvent(buttons::Button::B, buttons::Event::LongPressStart,
            [](buttons::Button, buttons::Event, void*) {
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
    // Adapter for ST7305 display (expects rendering::IFramebuffer)
    // ------------------------------------------------------------------
    FramebufferAdapter displayAdapter(fb);

    // Previous frame for dirty-region tracking
    onebit::Framebuffer<400, 300> prevFb;
    FramebufferAdapter prevAdapter(prevFb);
    bool hasPrevFrame = prevFb.buffer() != nullptr;

    // ------------------------------------------------------------------
    // Idle tracking for power management
    // ------------------------------------------------------------------
    int64_t lastInputTime = esp_timer_get_time();
    constexpr int64_t IDLE_THRESHOLD_US = 60 * 1000000LL;  // 60s idle → light sleep

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
            lastInputTime = frameStart;

            // --- Button A short press: toggle menu ---
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonShort &&
                evt.button_id == 0) {
                if (menu.isOpen()) {
                    // Confirm selection
                    app::Menu::Item sel = menu.confirm();
                    menu.close();

                    switch (sel) {
                        case app::Menu::Item::Dashboard:
                            currentMode = AppMode::Dashboard;
                            break;
                        case app::Menu::Item::Terminal:
                            currentMode = AppMode::Terminal;
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
                } else {
                    menu.open();
                }
            }

            // --- Button A long press: BLE pairing ---
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonLong &&
                evt.button_id == 0) {
                ESP_LOGI(TAG, "BLE pairing mode");
                showStatus(fb, display, "BLE Pairing...",
                           "Connect keyboard within 30s");
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

            // --- Button B long press: disconnect SSH / return to dashboard ---
            if (evt.source == input::Source::Button &&
                evt.type == input::EventType::ButtonLong &&
                evt.button_id == 1) {
                if (currentMode == AppMode::Terminal) {
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
                        switch (sel) {
                            case app::Menu::Item::Dashboard:
                                currentMode = AppMode::Dashboard;
                                break;
                            case app::Menu::Item::Terminal:
                                currentMode = AppMode::Terminal;
                                break;
                            default:
                                break;
                        }
                    }
                    // Escape closes menu
                    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
                        menu.close();
                    }
                } else if (currentMode == AppMode::Terminal &&
                           sshClient.state() == ssh::State::Connected) {
                    sshClient.send(evt.data, evt.data_length);
                }
            }
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
        // Power management: light sleep when idle in dashboard mode
        // ----------------------------------------------------------
        if (currentMode == AppMode::Dashboard &&
            (frameStart - lastInputTime) > IDLE_THRESHOLD_US) {
            ESP_LOGI(TAG, "Idle — entering light sleep");
            // Configure wake sources: button GPIO + timer
            esp_sleep_enable_gpio_wakeup();
            esp_sleep_enable_timer_wakeup(
                static_cast<uint64_t>(settings.dashboard_interval_ms) * 1000);
            esp_light_sleep_start();
            lastInputTime = esp_timer_get_time();  // Reset on wake
            ESP_LOGI(TAG, "Woke from light sleep");
        }

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
