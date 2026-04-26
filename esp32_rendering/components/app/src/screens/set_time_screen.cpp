#include "screens/set_time_screen.hpp"
#include "time_service.hpp"
#include "screen_stack.hpp"
#include <1bit/render/primitives.hpp>
#include <ctime>
#include <cstdio>

namespace app {

SetTimeScreen::SetTimeScreen(ScreenContext& ctx, bool from_wizard)
    : ctx_(ctx), from_wizard_(from_wizard) {
    if (ctx_.timeService.isTimeValid()) {
        time_t now = time(nullptr);
        struct tm tm_local{};
        localtime_r(&now, &tm_local);
        t_.year    = static_cast<uint16_t>(tm_local.tm_year + 1900);
        t_.month   = static_cast<uint8_t>(tm_local.tm_mon + 1);
        t_.day     = static_cast<uint8_t>(tm_local.tm_mday);
        t_.hour    = static_cast<uint8_t>(tm_local.tm_hour);
        t_.minute  = static_cast<uint8_t>(tm_local.tm_min);
        t_.second  = static_cast<uint8_t>(tm_local.tm_sec);
        t_.weekday = static_cast<uint8_t>(tm_local.tm_wday);
    } else {
        t_ = sensors::RtcTime{/*year*/2026, /*month*/1, /*day*/1,
                              /*hour*/0,  /*minute*/0, /*second*/0,
                              /*weekday*/0};
    }
}

static int daysInMonth(uint16_t y, uint8_t m) {
    static const int kDays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 31;
    if (m == 2) {
        bool leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
        return leap ? 29 : 28;
    }
    return kDays[m - 1];
}

void SetTimeScreen::clampDay() {
    int dim = daysInMonth(t_.year, t_.month);
    if (t_.day > dim) t_.day = static_cast<uint8_t>(dim);
    if (t_.day < 1) t_.day = 1;
}

void SetTimeScreen::adjust(int delta) {
    switch (field_) {
        case 0: { int v = (int)t_.year + delta; if (v < 2024) v = 2099; if (v > 2099) v = 2024; t_.year = static_cast<uint16_t>(v); break; }
        case 1: { int v = (int)t_.month + delta; if (v < 1) v = 12; if (v > 12) v = 1; t_.month = static_cast<uint8_t>(v); break; }
        case 2: { int v = (int)t_.day + delta; int dim = daysInMonth(t_.year, t_.month); if (v < 1) v = dim; if (v > dim) v = 1; t_.day = static_cast<uint8_t>(v); break; }
        case 3: { int v = (int)t_.hour + delta; if (v < 0) v = 23; if (v > 23) v = 0; t_.hour = static_cast<uint8_t>(v); break; }
        case 4: { int v = (int)t_.minute + delta; if (v < 0) v = 59; if (v > 59) v = 0; t_.minute = static_cast<uint8_t>(v); break; }
        case 5: { int v = (int)t_.second + delta; if (v < 0) v = 59; if (v > 59) v = 0; t_.second = static_cast<uint8_t>(v); break; }
    }
    clampDay();
}

void SetTimeScreen::render(onebit::IFramebuffer& fb,
                           const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    onebit::drawBitmapText(fb, font, 10, 20, "Set time", onebit::BLACK);

    char line1[32];
    char line2[32];
    std::snprintf(line1, sizeof(line1), "%04u-%02u-%02u",
                  t_.year, t_.month, t_.day);
    std::snprintf(line2, sizeof(line2), "%02u:%02u:%02u",
                  t_.hour, t_.minute, t_.second);
    onebit::drawBitmapText(fb, font, 10, 60, line1, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 90, line2, onebit::BLACK);

    // Underline the active field. Glyph cell width is `font.glyph_width + 1`.
    // Field x-offset (in glyphs) within the line: Y=0, M=5, D=8, h=0, m=3, s=6.
    const int16_t cw = font.glyph_width + 1;
    static const int kXglyph[] = {0, 5, 8, 0, 3, 6};
    static const int kLenGlyph[] = {4, 2, 2, 2, 2, 2};
    static const int kY[]      = {72, 72, 72, 102, 102, 102};
    int16_t ux = static_cast<int16_t>(10 + kXglyph[field_] * cw);
    int16_t uw = static_cast<int16_t>(kLenGlyph[field_] * cw);
    onebit::fillRect(fb, ux, kY[field_], uw, 1, onebit::BLACK);

    onebit::drawBitmapText(fb, font, 10, 200,
                           "L/R field  Up/Dn adjust", onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 215,
                           "Enter save  Esc cancel",  onebit::BLACK);
}

void SetTimeScreen::handleInput(const input::InputEvent& evt,
                                ScreenStack& stack) {
    using ET = input::EventType;
    // Buttons: A short = next field, B short = -1, B long = save (Enter),
    // A long = +1.
    if (evt.source == input::Source::Button && evt.type == ET::ButtonShort) {
        if (evt.button_id == 0) field_ = (field_ + 1) % 6;
        else                    adjust(-1);
        return;
    }
    if (evt.source == input::Source::Button && evt.type == ET::ButtonLong) {
        if (evt.button_id == 0) {
            adjust(+1);
        } else {
            if (ctx_.timeService.setManual(t_)) stack.pop();
        }
        return;
    }
    if (evt.source != input::Source::Keyboard || evt.type != ET::Keypress) return;
    // ESC sequence: ESC [ A/B/C/D
    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        switch (evt.data[2]) {
            case 'A': adjust(+1); return;
            case 'B': adjust(-1); return;
            case 'C': field_ = (field_ + 1) % 6; return;
            case 'D': field_ = (field_ + 5) % 6; return;
        }
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == '\r') {
        if (ctx_.timeService.setManual(t_)) stack.pop();
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
    }
}

}  // namespace app
