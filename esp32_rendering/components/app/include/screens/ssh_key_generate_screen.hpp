#pragma once

#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

/// Wi-Fi-gated Ed25519 key generation. Phase 13 Commit B fills in the
/// full TextInput-driven flow; this header already reflects the final
/// constructor signature so call-sites don't need to change.
class SshKeyGenerateScreen : public Screen {
public:
    explicit SshKeyGenerateScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
};

} // namespace app
