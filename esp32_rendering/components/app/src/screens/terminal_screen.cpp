#include "screens/terminal_screen.hpp"
#include "screens/dashboard_screen.hpp"
#include "screens/menu_screen.hpp"
#include "screen_stack.hpp"
#include "settings.hpp"
#include "overlay.hpp"
#include "font_for_size.hpp"
#include "ssh_client.hpp"
#include <array>
#include <esp_log.h>

static const char* TAG = "term_screen";

namespace app {

namespace {
constexpr std::array<app::Command, 3> kContextual = {{
    {"Send Ctrl+C",      "",            0xFF61},
    {"Disconnect",       "",            0xFF62},
    {"Cycle font size",  "Btn B short", 0xFF63},
}};
} // namespace

app::SpanView<const app::Command> TerminalScreen::getContextualCommands() {
    return app::SpanView<const app::Command>(kContextual.data(),
                                              kContextual.size());
}

void TerminalScreen::dispatchContextual(uint16_t id) {
    switch (id) {
        case 0xFF61: {
            // Forward a single 0x03 (Ctrl+C / SIGINT) byte down the SSH
            // channel — same path the keyboard handler uses.
            const uint8_t etx = 0x03;
            ctx_.sshClient.send(&etx, 1);
            break;
        }
        case 0xFF62:
            ctx_.sshClient.disconnect();
            ctx_.stack.replace(std::make_unique<DashboardScreen>(ctx_));
            break;
        case 0xFF63:
            ctx_.currentFontSize = (ctx_.currentFontSize + 1) % 3;
            ctx_.settings.font_size = ctx_.currentFontSize;
            ctx_.terminalMode.setFont(app::fontForSize(ctx_.currentFontSize));
            if (!app::saveSettings(ctx_.settings)) {
                ctx_.overlay.showError("Settings save failed",
                                        "NVS write error");
            }
            break;
        default: break;
    }
}

TerminalScreen::TerminalScreen(ScreenContext& ctx) : ctx_(ctx) {}

void TerminalScreen::onEnter() {
    ESP_LOGI(TAG, "TerminalScreen entered");
}

void TerminalScreen::feedSshData(const uint8_t* data, size_t len) {
    ctx_.terminalMode.feedData(data, len);
}

void TerminalScreen::handleInput(const input::InputEvent& evt,
                                 ScreenStack& stack) {
    // Button A short → push MenuScreen
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 0) {
        stack.push(std::make_unique<MenuScreen>(ctx_));
        return;
    }

    // Button B short → cycle font size (preserves existing behavior).
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 1) {
        ctx_.currentFontSize = (ctx_.currentFontSize + 1) % 3;
        ctx_.settings.font_size = ctx_.currentFontSize;
        ctx_.terminalMode.setFont(app::fontForSize(ctx_.currentFontSize));
        if (!app::saveSettings(ctx_.settings)) {
            ctx_.overlay.showError("Settings save failed", "NVS write error");
        }
        return;
    }

    // Button B long → switch to Dashboard (preserves legacy semantic).
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 1) {
        stack.replace(std::make_unique<DashboardScreen>(ctx_));
        return;
    }

    // Keyboard → SSH passthrough, with F1 (ESC O P) intercepted for menu.
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        bool isF1 = (evt.data_length == 3 &&
                     evt.data[0] == 0x1B &&
                     evt.data[1] == 'O' &&
                     evt.data[2] == 'P');
        if (isF1) {
            stack.push(std::make_unique<MenuScreen>(ctx_));
            return;
        }
        ctx_.sshClient.send(evt.data, evt.data_length);
    }
}

void TerminalScreen::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont&) {
    (void)fb;
    // TerminalMode::render() writes to its constructor-held fb ref.
    // Accepted wart — TerminalScreen is a thin pass-through.
    ctx_.terminalMode.render();
}

} // namespace app
