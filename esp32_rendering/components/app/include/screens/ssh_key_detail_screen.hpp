#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "ssh_key_types.hpp"

namespace app {

/// Per-key detail view. Hotkeys: T=text, Q=QR, S=SD export, E=Enroll,
/// R=Rename, D=Delete, Esc=back. Phase 13 Commit B fills in the full
/// render + dispatch.
class SshKeyDetailScreen : public Screen {
public:
    SshKeyDetailScreen(ScreenContext& ctx, const ssh_keys::KeyId& id);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    ssh_keys::KeyId id_;
};

} // namespace app
