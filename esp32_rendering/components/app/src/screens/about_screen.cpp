#include "screens/about_screen.hpp"
#include "screen_stack.hpp"
#include <1bit/render/primitives.hpp>
#include <array>

namespace app {

namespace {
constexpr std::array<app::KeybindHint, 2> kHints = {{
    {"Esc",    "close"},
    {"Ctrl+/", "help"},
}};
static_assert(sizeof("Ctrl+/") <= 12 && sizeof("close") <= 16,
              "kHints contains a string longer than KeybindHint capacity");
} // namespace

app::SpanView<const app::KeybindHint> AboutScreen::keybindHints() const {
    return kHints;
}

void AboutScreen::handleInput(const input::InputEvent& evt,
                              ScreenStack& stack) {
    (void)ctx_;
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress &&
        evt.data_length >= 1 &&
        (evt.data[0] == '\r' || evt.data[0] == 0x1B)) {
        stack.pop();
    } else if (evt.source == input::Source::Button &&
               (evt.type == input::EventType::ButtonShort ||
                evt.type == input::EventType::ButtonLong)) {
        stack.pop();
    }
}

void AboutScreen::render(onebit::IFramebuffer& fb,
                         const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 20,
                           "RLCD Terminal v1.0", onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 40,
                           "github.com/Devin-Cooper/RLCD", onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 70,
                           "[ Enter / Esc / any key to close ]",
                           onebit::BLACK);
}

} // namespace app
