#include "screens/ssh_key_generate_screen.hpp"
#include "screen_stack.hpp"

#include <1bit/render/primitives.hpp>

namespace app {

SshKeyGenerateScreen::SshKeyGenerateScreen(ScreenContext& ctx) : ctx_(ctx) {
    (void)ctx_;
}

void SshKeyGenerateScreen::onEnter() {}

void SshKeyGenerateScreen::handleInput(const input::InputEvent& evt,
                                        ScreenStack& stack) {
    if (evt.source == input::Source::Keyboard &&
        evt.type   == input::EventType::Keypress &&
        evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
    }
}

void SshKeyGenerateScreen::render(onebit::IFramebuffer& fb,
                                   const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8,
                            "Generate Key (Phase 13 stub)", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 8 + font.glyph_height + 12,
                            "Not implemented yet. Esc to return.",
                            onebit::BLACK);
}

} // namespace app
