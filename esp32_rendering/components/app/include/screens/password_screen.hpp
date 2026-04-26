#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "text_input.hpp"
#include "wifi_manager.hpp"

namespace app {

class PasswordScreen : public Screen {
public:
    PasswordScreen(ScreenContext& ctx, const char* ssid);

    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

private:
    ScreenContext& ctx_;
    char ssid_[33];
    char pw_[64];
    TextInput input_;
    bool connecting_ = false;
};

} // namespace app
