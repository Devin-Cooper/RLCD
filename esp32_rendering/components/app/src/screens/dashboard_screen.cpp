#include "screens/dashboard_screen.hpp"
#include "screens/menu_screen.hpp"
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
    // Button A short → push MenuScreen
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 0) {
        stack.push(std::make_unique<MenuScreen>(ctx_));
        return;
    }

    // Button B long → switch to next server (existing power-user shortcut).
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 1) {
        if (ctx_.switchToNextServer) ctx_.switchToNextServer();
        return;
    }
}

void DashboardScreen::render(onebit::IFramebuffer& fb,
                             const onebit::BitmapFont& font) {
    ctx_.dashboard.render(fb, font);
}

} // namespace app
