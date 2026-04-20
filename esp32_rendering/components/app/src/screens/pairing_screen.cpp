#include "screens/pairing_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "ble_hid.hpp"
#include <1bit/render/primitives.hpp>
#include <esp_timer.h>
#include <esp_log.h>

static const char* TAG = "pair_screen";

namespace app {

PairingScreen::PairingScreen(ScreenContext& ctx) : ctx_(ctx) {}

void PairingScreen::onEnter() {
    ESP_LOGI(TAG, "PairingScreen entered; starting BLE pairing");
    ctx_.bleHost.startPairing(30);
    started_us_ = esp_timer_get_time();
}

void PairingScreen::handleInput(const input::InputEvent& evt,
                                ScreenStack& stack) {
    // While top: ignore Btn A (suppress menu open during pairing).
    if (evt.source == input::Source::Button) return;
    // BLE state synthetic event — drives auto-pop
    if (evt.source == input::Source::System &&
        evt.type == input::EventType::BleStateChanged) {
        auto new_state = static_cast<ble_hid::State>(evt.data[0]);
        if (new_state == ble_hid::State::Connected) {
            ctx_.overlay.showToast("Keyboard connected", 2000);
            stack.pop();
        } else if (new_state == ble_hid::State::Disconnected) {
            ctx_.overlay.showToast("No keyboard found", 2500);
            stack.pop();
        }
    }
}

void PairingScreen::render(onebit::IFramebuffer& fb,
                           const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 10, "BLE Pairing Mode", onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 30,
                           "Put keyboard in pairing mode", onebit::BLACK);

    const char* state_str = "Scanning...";
    auto ble_state = ctx_.bleHost.state();
    if (ble_state == ble_hid::State::Connecting) state_str = "Connecting...";
    else if (ble_state == ble_hid::State::Connected) state_str = "Connected!";
    else if (ble_state == ble_hid::State::Disconnected)
        state_str = "Timeout — no keyboard found";
    onebit::drawBitmapText(fb, font, 10, 50, state_str, onebit::BLACK);

    char dbg[48];
    int64_t elapsed_s = (esp_timer_get_time() - started_us_) / 1000000;
    snprintf(dbg, sizeof(dbg), "Elapsed: %llds", (long long)elapsed_s);
    onebit::drawBitmapText(fb, font, 10, 70, dbg, onebit::BLACK);
}

} // namespace app
