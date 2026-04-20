#include "screens/dashboard_screen.hpp"
#include "screen_stack.hpp"
#include <esp_log.h>

static const char* TAG = "dash_screen";

namespace app {

DashboardScreen::DashboardScreen(ScreenContext& ctx) : ctx_(ctx) {}

void DashboardScreen::onEnter() {
    ESP_LOGI(TAG, "DashboardScreen entered");
}

void DashboardScreen::tickUpdate(int64_t now_ms) {
    ctx_.dashboard.update(ctx_.sshClient, now_ms);
}

void DashboardScreen::feedSshData(const uint8_t* data, size_t len) {
    ctx_.dashboard.feedData(data, len);
}

void DashboardScreen::handleInput(const input::InputEvent& evt,
                                  ScreenStack& stack) {
    (void)stack;  // Menu open uses openLegacyMenu during 2a/2b; MenuScreen push lands at Task 11.
    // Button A short → open menu (legacy bridge; replaced at Task 11)
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 0) {
        if (ctx_.openLegacyMenu) ctx_.openLegacyMenu();
        return;
    }
    // Button B long → switch to next server (existing power-user shortcut).
    // Still handled by main.cpp's legacy dispatch until Task 14 rewires
    // via ctx_.switchToActiveServer (populated at Task 14).
}

void DashboardScreen::render(onebit::IFramebuffer& fb,
                             const onebit::BitmapFont& font) {
    ctx_.dashboard.render(fb, font);
}

} // namespace app
