#include "screens/password_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <cstdio>

namespace app {

static bool isTerminalDisconnectReason(uint8_t reason) {
    // Reasons that indicate we should stop waiting for a connect and
    // show the user a Connect Failed modal. Reason code 0 means the
    // WifiManager callback didn't thread the reason through (main.cpp
    // sets data[1]=0 until Amendment C1's reason-threading lands);
    // treat 0 as "probably terminal" to preserve existing behavior.
    // Terminal reasons: NO_AP_FOUND(201), AUTH_FAIL(202), ASSOC_FAIL(203),
    // HANDSHAKE_TIMEOUT(204), 4WAY_HANDSHAKE_TIMEOUT(15).
    return reason == 0 || reason == 15 ||
           reason == 201 || reason == 202 ||
           reason == 203 || reason == 204;
}

PasswordScreen::PasswordScreen(ScreenContext& ctx, const char* ssid)
    : ctx_(ctx), input_(pw_, sizeof(pw_),
                        TextInputOpts{.masked = true,
                                      .tab_toggles_reveal = true}) {
    std::strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';
    pw_[0] = '\0';
}

void PasswordScreen::handleInput(const input::InputEvent& evt, ScreenStack& stack) {
    if (evt.source == input::Source::System &&
        evt.type == input::EventType::WifiStateChanged) {
        auto s = static_cast<wifi::State>(evt.data[0]);
        uint8_t reason = (evt.data_length >= 2) ? evt.data[1] : 0;
        if (connecting_ && s == wifi::State::Connected) {
            connecting_ = false;
            ctx_.wifiMgr.saveNetwork(ssid_, pw_);
            char toast[64];
            snprintf(toast, sizeof(toast), "Connected to %s", ssid_);
            ctx_.overlay.showToast(toast, 2500);
            stack.pop();   // password
            stack.pop();   // wifi
        } else if (connecting_ && s == wifi::State::Disconnected &&
                   isTerminalDisconnectReason(reason)) {
            connecting_ = false;
            ctx_.overlay.showError("Connect failed",
                "Wrong password or network unreachable");
            // buffer preserved so user can edit
        }
        // Non-terminal disconnect reasons: keep waiting.
        return;
    }

    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        TextInputResult r = input_.handleKey(evt.data, evt.data_length);
        if (r == TextInputResult::Submit) {
            connecting_ = true;
            ctx_.wifiMgr.connect(ssid_, pw_);
            ctx_.overlay.showToast("Connecting...", 1500);
        } else if (r == TextInputResult::Cancel) {
            stack.pop();
        }
    }
}

void PasswordScreen::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    char line[96];
    snprintf(line, sizeof(line), "Connect to: %s", ssid_);
    onebit::drawBitmapText(fb, font, 10, 10, line, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 30, "Password:", onebit::BLACK);
    input_.render(fb, font, 10, 50, fb.width() - 20);

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Enter connect  Esc cancel  Tab/Ctrl+R show", onebit::BLACK);
}

} // namespace app
