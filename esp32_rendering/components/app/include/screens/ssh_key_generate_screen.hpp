#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "text_input.hpp"

#include <string>

namespace app {

/// Wi-Fi-gated Ed25519 key generation. A single `TextInput` field prompts
/// for the name. On Submit: optionally show the one-time plaintext-warning
/// modal, then call `ssh_keys::generate_ed25519`. Result is surfaced via
/// Toast (success) or Modal (errors).
class SshKeyGenerateScreen : public Screen {
public:
    explicit SshKeyGenerateScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    char name_buf_[32] = {};
    TextInput input_;
    bool wifi_ok_ = false;  // set in onEnter
    bool generating_ = false;

    void submit(ScreenStack& stack);
};

} // namespace app
