#pragma once
#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

class AboutScreen : public Screen {
public:
    explicit AboutScreen(ScreenContext& ctx) : ctx_(ctx) {}
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;
private:
    ScreenContext& ctx_;
};

} // namespace app
