#include "screens/keyboard_gate.hpp"
#include "screens/pairing_screen.hpp"
#include "animator.hpp"
#include <1bit/render/primitives.hpp>
#include <esp_timer.h>

namespace app {

KeyboardGateModal::KeyboardGateModal(ScreenContext& ctx,
                                     std::unique_ptr<Screen> deferred)
    : ctx_(ctx), deferred_(std::move(deferred)) {}

void KeyboardGateModal::onEnter() {
    auto tag = makeTag(TweenKind::ModalScale, 0);
    ctx_.animator.start(tag, 0, 100, kModalScaleUs, esp_timer_get_time());
}

void KeyboardGateModal::handleInput(const input::InputEvent& evt, ScreenStack& stack) {
    bool cancel = false;
    bool accept = false;
    if (evt.source == input::Source::Button) {
        if (evt.type == input::EventType::ButtonShort && evt.button_id == 0) cancel = true;
        if (evt.type == input::EventType::ButtonLong  && evt.button_id == 0) accept = true;
        if (evt.type == input::EventType::ButtonLong  && evt.button_id == 1) accept = true;
    }
    if (evt.source == input::Source::Keyboard
        && evt.type == input::EventType::Keypress
        && evt.data_length >= 1) {
        if (evt.data[0] == 0x1B) cancel = true;        // Esc
        if (evt.data[0] == 0x03) cancel = true;        // Ctrl+C
        if (evt.data_length == 1
            && (evt.data[0] == '\r' || evt.data[0] == '\n')) accept = true;
    }
    if (cancel) {
        deferred_.reset();
        stack.pop();
        return;
    }
    if (accept) {
        // Amendment D: replace gate modal with PairingScreen carrying the
        // deferred screen — the gate is GONE from the stack so a successful
        // pair lands the user directly on the deferred screen.
        stack.replaceBypassingGate(
            ScreenStack::BypassToken{},
            std::make_unique<PairingScreen>(ctx_, std::move(deferred_)));
        return;
    }
}

void KeyboardGateModal::render(onebit::IFramebuffer& fb,
                                const onebit::BitmapFont& font) {
    auto tag = makeTag(TweenKind::ModalScale, 0);
    int16_t scale = ctx_.animator.value(tag, esp_timer_get_time());
    if (scale <= 0) return;
    int16_t full_w = 280, full_h = 120;
    int16_t cur_w = static_cast<int16_t>((full_w * scale) / 100);
    int16_t cur_h = static_cast<int16_t>((full_h * scale) / 100);
    int16_t fb_w = fb.width();
    int16_t fb_h = fb.height();
    int16_t x = (fb_w - cur_w) / 2;
    int16_t y = (fb_h - cur_h) / 2;
    onebit::fillRect(fb, x, y, cur_w, cur_h, onebit::WHITE);
    onebit::drawRect(fb, x, y, cur_w, cur_h, onebit::BLACK);
    if (scale < 80) return;
    int16_t row_y = y + 8;
    onebit::drawBitmapText(fb, font, x + 8, row_y, "Pair a keyboard", onebit::BLACK);
    row_y += font.glyph_height + 6;
    onebit::drawBitmapText(fb, font, x + 8, row_y,
                           "This screen needs a keyboard.", onebit::BLACK);
    row_y += font.glyph_height + 6;
    onebit::drawBitmapText(fb, font, x + 8, row_y,
                           "[Pair]   Btn B long-press", onebit::BLACK);
    row_y += font.glyph_height + 2;
    onebit::drawBitmapText(fb, font, x + 8, row_y,
                           "[Cancel] Btn A short-press", onebit::BLACK);
    row_y += font.glyph_height + 2;
    onebit::drawBitmapText(fb, font, x + 8, row_y,
                           "(or Enter / Esc)", onebit::BLACK);
}

} // namespace app
