#include "screens/text_input_screen.hpp"
#include "screen_stack.hpp"

#include <1bit/render/primitives.hpp>

#include <cstring>

namespace app {

TextInputScreen::TextInputScreen(ScreenContext& ctx,
                                 const char* title,
                                 const char* initial,
                                 Callback on_done,
                                 TextInputOpts opts)
    : ctx_(ctx),
      input_(buffer_, sizeof(buffer_), opts),
      on_done_(std::move(on_done)) {
    (void)ctx_;
    title_[0] = '\0';
    buffer_[0] = '\0';
    if (title) {
        std::strncpy(title_, title, sizeof(title_) - 1);
        title_[sizeof(title_) - 1] = '\0';
    }

    // Seed initial by pumping bytes through the widget so its internal
    // cursor_ stays in sync with the buffer contents.
    if (initial && initial[0] != '\0') {
        size_t n = std::strlen(initial);
        for (size_t i = 0; i < n; ++i) {
            uint8_t b = static_cast<uint8_t>(initial[i]);
            // Skip any byte that TextInput treats specially.
            if (b == '\r' || b == '\n' || b == 0x1B ||
                b == 0x08 || b == 0x7F || b == '\t') continue;
            if (b < 0x20 || b > 0x7E) continue;
            input_.handleKey(&b, 1);
        }
    }
}

void TextInputScreen::handleInput(const input::InputEvent& evt,
                                  ScreenStack& stack) {
    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) {
        return;
    }

    // Direct Esc handling — route to Cancel regardless of widget state.
    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        if (on_done_) on_done_(TextInputResult::Cancel, std::string{});
        stack.pop();
        return;
    }

    TextInputResult r = input_.handleKey(evt.data, evt.data_length);
    if (r == TextInputResult::Submit) {
        std::string value(buffer_);
        if (on_done_) on_done_(TextInputResult::Submit, value);
        stack.pop();
    } else if (r == TextInputResult::Cancel) {
        if (on_done_) on_done_(TextInputResult::Cancel, std::string{});
        stack.pop();
    }
}

void TextInputScreen::render(onebit::IFramebuffer& fb,
                             const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8, title_, onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 12;
    input_.render(fb, font, 10, y, fb.width() - 20);

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
                            "Enter save  Esc cancel", onebit::BLACK);
}

} // namespace app
