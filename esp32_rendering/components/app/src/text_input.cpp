#include "text_input.hpp"
#include <1bit/render/primitives.hpp>
#include <esp_timer.h>
#include <cctype>
#include <cstring>

namespace app {

TextInput::TextInput(char* buffer, size_t capacity, TextInputOpts opts)
    : buffer_(buffer), capacity_(capacity), cursor_(0), opts_(opts) {
    if (buffer_ && capacity_) buffer_[0] = '\0';
    cursor_blink_epoch_us_ = esp_timer_get_time();
}

void TextInput::clear() {
    cursor_ = 0;
    if (buffer_ && capacity_) buffer_[0] = '\0';
    revealed_ = false;
}

TextInputResult TextInput::handleKey(const uint8_t* data, size_t len) {
    if (!data || len == 0) return TextInputResult::None;

    // Multi-byte — ignore escape sequences (arrow keys, function keys, etc.).
    if (len > 1) return TextInputResult::None;

    // len == 1
    uint8_t b = data[0];
    if (b == '\r' || b == '\n') return TextInputResult::Submit;
    if (b == 0x1B) return TextInputResult::Cancel;
    if (b == 0x08 || b == 0x7F) {
        if (cursor_ > 0) {
            cursor_--;
            buffer_[cursor_] = '\0';
        }
        return TextInputResult::None;
    }
    if (b == '\t' && opts_.masked && opts_.tab_toggles_reveal) {
        toggleReveal();
        return TextInputResult::None;
    }
    if (b == KEY_CTRL_R && opts_.masked) {
        toggleReveal();
        return TextInputResult::None;
    }
    if (b >= 0x20 && b <= 0x7E) {
        if (opts_.numeric && !std::isdigit(b))
            return TextInputResult::None;
        if (cursor_ + 1 >= capacity_)
            return TextInputResult::None;
        buffer_[cursor_++] = static_cast<char>(b);
        buffer_[cursor_]   = '\0';
    }
    return TextInputResult::None;
}

void TextInput::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                       int16_t x, int16_t y, int16_t width) {
    const int16_t h = font.glyph_height + 4;
    onebit::fillRect(fb, x, y, width, h, onebit::WHITE);
    onebit::drawRect(fb, x, y, width, h, onebit::BLACK);

    char disp[96];
    if (opts_.masked && !revealed_) {
        for (size_t i = 0; i < cursor_ && i < sizeof(disp) - 1; ++i)
            disp[i] = '*';
        disp[cursor_ < sizeof(disp) - 1 ? cursor_ : sizeof(disp) - 1] = '\0';
    } else {
        std::strncpy(disp, buffer_, sizeof(disp) - 1);
        disp[sizeof(disp) - 1] = '\0';
    }

    onebit::drawBitmapText(fb, font, x + 2, y + 2, disp, onebit::BLACK);

    int64_t period = 1000000;
    int64_t now = esp_timer_get_time();
    bool on = ((now - cursor_blink_epoch_us_) % period) < (period / 2);
    if (on) {
        int16_t cursor_x = x + 2 + onebit::getBitmapTextWidth(font, disp);
        int16_t cursor_y = y + 2 + font.glyph_height - 1;
        int16_t cw = font.glyph_width;
        onebit::fillRect(fb, cursor_x, cursor_y, cw, 2, onebit::BLACK);
    }
}

} // namespace app
