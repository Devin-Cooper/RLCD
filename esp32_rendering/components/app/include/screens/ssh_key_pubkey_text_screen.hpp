#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "ssh_key_types.hpp"

#include <string>

namespace app {

/// Wrapped OpenSSH `ssh-ed25519 AAAA… name` display. Phase 13 Commit B
/// fills in the wrap + render from `ssh_keys::wrapped_pubkey_line`.
class SshKeyPubkeyTextScreen : public Screen {
public:
    SshKeyPubkeyTextScreen(ScreenContext& ctx, const ssh_keys::KeyId& id);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

private:
    ScreenContext& ctx_;
    ssh_keys::KeyId id_;
    std::string text_;
};

} // namespace app
