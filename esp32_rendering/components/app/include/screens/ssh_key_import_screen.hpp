#pragma once

#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

/// SD-card import: scans /sdcard/ssh_keys/*.pem, previews each via libssh,
/// and lets the user pick one to name and add. Phase 13 Commit C fills in
/// the full scan/candidate/add flow.
class SshKeyImportScreen : public Screen {
public:
    explicit SshKeyImportScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
};

} // namespace app
