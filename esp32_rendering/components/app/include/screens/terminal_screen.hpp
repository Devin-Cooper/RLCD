#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "terminal_mode.hpp"

namespace app {

class TerminalScreen : public Screen {
public:
    explicit TerminalScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    int targetFps() const override { return 30; }

    bool bypassesKeyboardGate() const override { return true; }
    ScreenKind screenKind() const override { return ScreenKind::Terminal; }
    const char* breadcrumbLabel() const override { return "Terminal"; }
    bool wantsKeybindFooter() const override { return false; }

    void feedSshData(const uint8_t* data, size_t len);

private:
    ScreenContext& ctx_;
};

} // namespace app
