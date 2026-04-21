#include "screens/ssh_key_enroll_screen.hpp"
#include "screen_stack.hpp"

#include <1bit/render/primitives.hpp>

namespace app {

SshKeyEnrollScreen::SshKeyEnrollScreen(ScreenContext& ctx,
                                        const ssh_keys::KeyId& id)
    : ctx_(ctx), id_(id) {
    (void)ctx_;
}

void SshKeyEnrollScreen::onEnter() {}

void SshKeyEnrollScreen::handleInput(const input::InputEvent& evt,
                                      ScreenStack& stack) {
    if (evt.source == input::Source::Keyboard &&
        evt.type   == input::EventType::Keypress &&
        evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
    }
}

void SshKeyEnrollScreen::render(onebit::IFramebuffer& fb,
                                 const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8,
                            "Enroll Key (Phase 13 stub)", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 8 + font.glyph_height + 12,
                            "Not implemented yet. Esc to return.",
                            onebit::BLACK);
}

} // namespace app
