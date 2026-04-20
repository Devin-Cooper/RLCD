#include "screens/settings_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "settings.hpp"
#include <1bit/render/primitives.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace app {

SettingsScreen::SettingsScreen(ScreenContext& ctx)
    : ctx_(ctx),
      ti_scrollback_(scrollback_, sizeof(scrollback_),
                     TextInputOpts{.numeric = true}),
      ti_dash_(dash_ms_, sizeof(dash_ms_),
               TextInputOpts{.numeric = true}) {}

void SettingsScreen::onEnter() {
    font_sel_ = ctx_.settings.font_size;
    snprintf(scrollback_, sizeof(scrollback_), "%u",
             ctx_.settings.scrollback_depth);
    snprintf(dash_ms_, sizeof(dash_ms_), "%u",
             ctx_.settings.dashboard_interval_ms);
    dirty_ = false;
    focus_ = 0;
}

void SettingsScreen::saveAndPop(ScreenStack& stack) {
    ctx_.settings.font_size = font_sel_;
    ctx_.settings.scrollback_depth =
        static_cast<uint16_t>(std::atoi(scrollback_));
    if (ctx_.settings.scrollback_depth == 0)
        ctx_.settings.scrollback_depth = 500;
    ctx_.settings.dashboard_interval_ms =
        static_cast<uint16_t>(std::atoi(dash_ms_));
    if (ctx_.settings.dashboard_interval_ms < 500)
        ctx_.settings.dashboard_interval_ms = 5000;

    if (app::saveSettings(ctx_.settings)) {
        ctx_.currentFontSize = font_sel_;
        ctx_.overlay.showToast("Settings saved", 2000);
        stack.pop();
    } else {
        ctx_.overlay.showError("Save failed", "NVS write error");
    }
}

void SettingsScreen::handleInput(const input::InputEvent& evt,
                                 ScreenStack& stack) {
    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) return;

    if (evt.data_length == 1 && evt.data[0] == '\t') {
        focus_ = (focus_ + 1) % 5; return;
    }
    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        char c = evt.data[2];
        if (c == 'A') { focus_ = (focus_ - 1 + 5) % 5; return; }
        if (c == 'B') { focus_ = (focus_ + 1) % 5; return; }
        if (focus_ == 0) {
            if (c == 'C') font_sel_ = (font_sel_ + 1) % 3;
            if (c == 'D') font_sel_ = (font_sel_ + 2) % 3;
            dirty_ = true;
            return;
        }
    }
    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop(); return;
    }
    if (evt.data_length == 1 && evt.data[0] == '\r') {
        if (focus_ == 3) { saveAndPop(stack); return; }
        if (focus_ == 4) { stack.pop(); return; }
        focus_ = (focus_ + 1) % 5;
        return;
    }
    if (focus_ == 1) ti_scrollback_.handleKey(evt.data, evt.data_length);
    if (focus_ == 2) ti_dash_.handleKey(evt.data, evt.data_length);
    dirty_ = true;
}

void SettingsScreen::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 6, "Settings", onebit::BLACK);

    // Font selector (row 0)
    int16_t y = 30;
    const char* fmark = (focus_ == 0) ? ">" : " ";
    onebit::drawBitmapText(fb, font, 8, y, fmark, onebit::BLACK);
    const char* labels[] = {"5x7", "6x9", "8x12"};
    int16_t x = 30;
    for (int i = 0; i < 3; ++i) {
        bool sel = (font_sel_ == i);
        int16_t w = onebit::getBitmapTextWidth(font, labels[i]) + 6;
        if (sel) {
            onebit::fillRect(fb, x - 2, y - 2, w, font.glyph_height + 4,
                             onebit::BLACK);
            onebit::drawBitmapText(fb, font, x, y, labels[i], onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, x, y, labels[i], onebit::BLACK);
        }
        x += w + 4;
    }

    y += font.glyph_height + 12;
    const char* sm = (focus_ == 1) ? ">" : " ";
    onebit::drawBitmapText(fb, font, 8, y, sm, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 30, y, "Scrollback:", onebit::BLACK);
    ti_scrollback_.render(fb, font, 130, y - 2, 80);

    y += font.glyph_height + 8;
    const char* dm = (focus_ == 2) ? ">" : " ";
    onebit::drawBitmapText(fb, font, 8, y, dm, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 30, y, "Dash refresh ms:", onebit::BLACK);
    ti_dash_.render(fb, font, 160, y - 2, 80);

    y += font.glyph_height + 16;
    auto drawBtn = [&](int16_t px, const char* txt, bool sel) {
        int16_t w = onebit::getBitmapTextWidth(font, txt) + 4;
        if (sel) {
            onebit::fillRect(fb, px - 2, y - 2, w, font.glyph_height + 4,
                             onebit::BLACK);
            onebit::drawBitmapText(fb, font, px, y, txt, onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, px, y, txt, onebit::BLACK);
        }
    };
    drawBtn(20, "[ Save ]", focus_ == 3);
    drawBtn(140, "[ Cancel ]", focus_ == 4);

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Tab next  Up/Dn nav  Left/Right font  Esc back", onebit::BLACK);
}

} // namespace app
