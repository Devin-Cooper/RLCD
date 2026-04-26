#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "text_input.hpp"

namespace app {

class ServerEditScreen : public Screen {
public:
    /// index == -1 → new server; else edit existing by index.
    ServerEditScreen(ScreenContext& ctx, int index);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

private:
    ScreenContext& ctx_;
    int index_;
    bool dirty_ = false;
    int focus_ = 0;  // 0..4 fields; 5 = Key; 6 = Save; 7 = Cancel

    char name_[32] = {};
    char host_[64] = {};
    char port_[6]  = {};
    char user_[32] = {};
    char pw_[64]   = {};

    TextInput ti_name_;
    TextInput ti_host_;
    TextInput ti_port_;
    TextInput ti_user_;
    TextInput ti_pw_;

    TextInput& activeField();
    void saveAndPop(ScreenStack& stack);
    void cancelWithConfirm(ScreenStack& stack);
};

} // namespace app
