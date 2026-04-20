#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "text_input.hpp"

namespace app {

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    uint8_t font_sel_ = 0;
    char scrollback_[6] = {};   // "65535" + NUL
    char dash_ms_[6] = {};
    TextInput ti_scrollback_;
    TextInput ti_dash_;
    int focus_ = 0;   // 0 font, 1 scrollback, 2 dash, 3 Save, 4 Cancel
    bool dirty_ = false;

    void saveAndPop(ScreenStack& stack);
};

} // namespace app
