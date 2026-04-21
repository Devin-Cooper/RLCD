#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "ssh_key_types.hpp"

namespace app {

/// SSH enrollment: pick a password-auth server, prompt for the SSH
/// password, then run `ssh_keys::enroll_key`. Phase 13 Commit D fills in
/// the full state machine.
class SshKeyEnrollScreen : public Screen {
public:
    SshKeyEnrollScreen(ScreenContext& ctx, const ssh_keys::KeyId& id);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    ssh_keys::KeyId id_;
};

} // namespace app
