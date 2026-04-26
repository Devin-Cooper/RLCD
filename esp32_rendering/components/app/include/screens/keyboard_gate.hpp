#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "screen_stack.hpp"
#include <memory>

namespace app {

class KeyboardGateModal : public Screen {
public:
    KeyboardGateModal(ScreenContext& ctx, std::unique_ptr<Screen> deferred);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    bool isTransparent() const override { return true; }
    bool wantsKeybindFooter() const override { return false; }
    bool bypassesKeyboardGate() const override { return true; }
    ScreenKind screenKind() const override { return ScreenKind::KeyboardGate; }
    const char* breadcrumbLabel() const override { return "Keyboard"; }

    /// Amendment F: token factory used by main.cpp to install the gate
    /// policy. Lives here because KeyboardGateModal is friended to
    /// ScreenStack::BypassToken's private default ctor.
    static ScreenStack::BypassToken installerToken() {
        return ScreenStack::BypassToken{};
    }

private:
    ScreenContext&            ctx_;
    std::unique_ptr<Screen>   deferred_;
};

} // namespace app
