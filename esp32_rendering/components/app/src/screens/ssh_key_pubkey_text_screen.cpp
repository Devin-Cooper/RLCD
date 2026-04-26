#include "screens/ssh_key_pubkey_text_screen.hpp"
#include "screen_stack.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_export.hpp"

#include <1bit/render/primitives.hpp>

#include <array>
#include <cstring>
#include <string>

namespace app {

namespace {
constexpr std::array<app::KeybindHint, 2> kHints = {{
    {"Esc",    "back"},
    {"Ctrl+/", "help"},
}};
static_assert(sizeof("Ctrl+/") <= 12 && sizeof("back") <= 16,
              "kHints contains a string longer than KeybindHint capacity");
} // namespace

app::SpanView<const app::KeybindHint> SshKeyPubkeyTextScreen::keybindHints() const {
    return kHints;
}

SshKeyPubkeyTextScreen::SshKeyPubkeyTextScreen(ScreenContext& ctx,
                                                const ssh_keys::KeyId& id)
    : ctx_(ctx), id_(id) {}

void SshKeyPubkeyTextScreen::onEnter() {
    text_ = ssh_keys::wrapped_pubkey_line(ctx_.keyStore, id_);
}

void SshKeyPubkeyTextScreen::handleInput(const input::InputEvent& evt,
                                          ScreenStack& stack) {
    if (evt.source == input::Source::Keyboard &&
        evt.type   == input::EventType::Keypress &&
        evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
    }
}

void SshKeyPubkeyTextScreen::render(onebit::IFramebuffer& fb,
                                     const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8, "Public Key", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 10;
    const int16_t row_h = font.glyph_height + 1;

    if (text_.empty()) {
        onebit::drawBitmapText(fb, font, 10, y,
                                "(no pubkey cache)", onebit::BLACK);
    } else {
        // Walk line-by-line splitting on '\n'.
        size_t start = 0;
        const size_t len = text_.size();
        while (start < len) {
            size_t nl = text_.find('\n', start);
            size_t end = (nl == std::string::npos) ? len : nl;
            if (y + font.glyph_height > fb.height() - font.glyph_height - 8) {
                break;
            }
            // Draw this slice — need a NUL-terminated buffer.
            char tmp[96];
            size_t n = end - start;
            if (n > sizeof(tmp) - 1) n = sizeof(tmp) - 1;
            std::memcpy(tmp, text_.data() + start, n);
            tmp[n] = '\0';
            onebit::drawBitmapText(fb, font, 10, y, tmp, onebit::BLACK);
            y += row_h;
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
                            "Esc to return", onebit::BLACK);
}

} // namespace app
