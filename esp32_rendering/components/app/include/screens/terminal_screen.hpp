#pragma once

#include <cstdint>

#include "screen.hpp"
#include "screen_context.hpp"
#include "command_registry.hpp"
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

    app::SpanView<const app::Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;

    void feedSshData(const uint8_t* data, size_t len);

private:
    enum class Mode : uint8_t { Live, Scrollback };

    ScreenContext& ctx_;
    Mode mode_           = Mode::Live;
    int  scroll_offset_  = 0;

    void enterScrollback(int initial_page_lines);
    void exitScrollback();
    void scrollBy(int delta_lines);
    void scrollToTop();
    void scrollToLive();             // also exits if successful
    void bounceUp();
    void bounceDown();
    void triggerBounce(int16_t from_dy);
    int  pageLines() const;
    int  maxOffset() const;
};

} // namespace app
