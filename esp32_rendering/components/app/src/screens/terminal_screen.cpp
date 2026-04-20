#include "screens/terminal_screen.hpp"
#include "screens/menu_screen.hpp"
#include "screen_stack.hpp"
#include "settings.hpp"
#include "overlay.hpp"
#include <esp_log.h>

static const char* TAG = "term_screen";

namespace app {

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
        // main.cpp owns the setFont call via its legacy dispatch path
        // during 2a/2b; Task 14 moves that here.
        app::saveSettings(ctx_.settings);
        return;
    }

    // Button B long → switch to Dashboard (existing terminal→dashboard
    // semantic). Still handled by main.cpp's legacy dispatch during 2a/2b.

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
