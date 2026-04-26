#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "ssh_key_types.hpp"

namespace app {

/// QR-code rendering of the pubkey line. Phase 13 Commit B calls
/// `ssh_keys::render_qr_to_framebuffer` per frame.
class SshKeyPubkeyQrScreen : public Screen {
public:
    SshKeyPubkeyQrScreen(ScreenContext& ctx, const ssh_keys::KeyId& id);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

private:
    ScreenContext& ctx_;
    ssh_keys::KeyId id_;
};

} // namespace app
