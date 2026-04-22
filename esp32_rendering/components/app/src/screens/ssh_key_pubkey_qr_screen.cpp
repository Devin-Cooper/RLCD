#include "screens/ssh_key_pubkey_qr_screen.hpp"
#include "screen_stack.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_export.hpp"

#include <1bit/render/primitives.hpp>

#include <cstdio>
#include <cstring>

namespace app {

SshKeyPubkeyQrScreen::SshKeyPubkeyQrScreen(ScreenContext& ctx,
                                            const ssh_keys::KeyId& id)
    : ctx_(ctx), id_(id) {}

void SshKeyPubkeyQrScreen::onEnter() {}

void SshKeyPubkeyQrScreen::handleInput(const input::InputEvent& evt,
                                        ScreenStack& stack) {
    if (evt.source == input::Source::Keyboard &&
        evt.type   == input::EventType::Keypress &&
        evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
    }
}

void SshKeyPubkeyQrScreen::render(onebit::IFramebuffer& fb,
                                   const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 4, "Pubkey QR", onebit::BLACK);

    // Clear the render region white — QR modules only flip pixels to black
    // where they're set; the "not-set" modules need a clean white
    // background to be scannable.
    int16_t body_y = 4 + font.glyph_height + 2;
    int16_t body_h = fb.height() - body_y - font.glyph_height - 8;
    onebit::fillRect(fb, 0, body_y, fb.width(), body_h, onebit::WHITE);

    // Origin for render_qr_to_framebuffer — picks scale from select_qr_scale
    // which budgets 300 px for (modules+8 quiet). Since we can't know the
    // actual module count without decoding first, pick an origin that
    // tolerates any V1-V17 output. V17 at scale 1 = 85 px. Center on the
    // top-half of the body so the footer fits underneath.
    //
    // select_qr_scale chooses based on the full 300px budget; V17 hits
    // scale=3 giving 85*3=255 px. Center horizontally around fb.width()/2
    // using the approximate V17 footprint; smaller versions produce
    // smaller grids with some off-center whitespace (fine).
    int approx_size = 85 * 3;  // V17 worst case
    int origin_x = (fb.width() - approx_size) / 2;
    int origin_y = body_y + 4;
    if (origin_x < 0) origin_x = 0;

    int scale = ssh_keys::render_qr_to_framebuffer(
        ctx_.keyStore, id_, &fb, origin_x, origin_y);
    if (scale == 0) {
        onebit::drawBitmapText(fb, font, 10, body_y + 20,
                                "QR too large for this key",
                                onebit::BLACK);
    }

    // Footer: fingerprint head in hex (12 chars).
    const auto* meta = ctx_.keyStore.find(id_);
    char footer[48] = {};
    if (meta) {
        char fp[16] = {};
        for (int k = 0; k < 6; ++k) {
            std::snprintf(fp + k * 2, 3, "%02x", meta->fp_sha256[k]);
        }
        std::snprintf(footer, sizeof(footer),
                       "sha256 head: %s  Esc", fp);
    } else {
        std::snprintf(footer, sizeof(footer), "Esc to return");
    }
    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
                            footer, onebit::BLACK);
}

} // namespace app
