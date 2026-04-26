#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "command_registry.hpp"
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

    app::SpanView<const app::KeybindHint> keybindHints() const override;

    app::SpanView<const app::Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;

private:
    ScreenContext& ctx_;
    uint8_t font_sel_ = 0;
    char scrollback_[6] = {};   // "65535" + NUL
    char dash_ms_[6] = {};
    TextInput ti_scrollback_;
    TextInput ti_dash_;
    // Focus rows: 0 font, 1 scrollback, 2 dash, 3 Date & Time,
    // 4 Time zone, 5 Save, 6 Cancel.
    int focus_ = 0;
    bool dirty_ = false;

    void saveAndPop(ScreenStack& stack);
};

} // namespace app
