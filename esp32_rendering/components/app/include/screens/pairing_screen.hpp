#pragma once
#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

class PairingScreen : public Screen {
public:
    explicit PairingScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    bool bypassesKeyboardGate() const override { return true; }
    ScreenKind screenKind() const override { return ScreenKind::Pairing; }
    const char* breadcrumbLabel() const override { return "Pairing"; }
    bool wantsKeybindFooter() const override { return false; }

private:
    ScreenContext& ctx_;
    int64_t started_us_ = 0;
};

} // namespace app
