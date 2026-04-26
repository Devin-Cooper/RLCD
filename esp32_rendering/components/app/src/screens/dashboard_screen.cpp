#include "screens/dashboard_screen.hpp"
#include "screens/menu_screen.hpp"
#include "screen_stack.hpp"
#include "time_service.hpp"
#include <1bit/render/primitives.hpp>
#include <esp_log.h>
#include <ctime>
#include <cstdio>
#include <cstring>

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

    // Overlay an "Updated HH:MM:SS" wall-clock timestamp inside the title-bar
    // region to the left of the dashboard's centered title. Uses the system
    // wall clock (set by SNTP, manual entry, or RTC). Kept as a separate
    // overlay so we don't have to thread TimeService into Dashboard::render.
    char ts[12];
    if (ctx_.timeService.isTimeValid()) {
        time_t now = time(nullptr);
        struct tm tm_local{};
        localtime_r(&now, &tm_local);
        std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d",
                      tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);
    } else {
        std::strcpy(ts, "--:--:--");
    }
    char buf[24];
    std::snprintf(buf, sizeof(buf), "Upd %s", ts);
    int tw = onebit::getBitmapTextWidth(font, buf, 1);
    // Title bar runs y=1..15 with diagonal stripes; knockout a white box for
    // the timestamp text. Anchor right of the close-box (which occupies
    // x=3..13) at x=20.
    onebit::fillRect(fb, 20 - 2, 1, tw + 4, 14, onebit::WHITE);
    onebit::drawBitmapText(fb, font, 20, 4, buf, onebit::BLACK, 1);
}

} // namespace app
