#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "text_input.hpp"

#include <functional>
#include <string>

namespace app {

/// Generic single-field text prompt. Reusable replacement for the plan's
/// assumed `TextInput`-as-Screen constructor — TextInput is only a widget,
/// so callers that want a full-screen prompt use this adapter.
///
/// The callback receives (Submit, final-value) or (Cancel, ""). The screen
/// pops itself after invoking the callback.
class TextInputScreen : public Screen {
public:
    using Callback = std::function<void(TextInputResult, const std::string&)>;

    TextInputScreen(ScreenContext& ctx,
                    const char* title,
                    const char* initial,
                    Callback on_done,
                    TextInputOpts opts = {});

    void onEnter() override {}
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    char title_[64];
    char buffer_[64];  // 64-char capacity; callers needing more must bump.
    TextInput input_;
    Callback on_done_;
};

} // namespace app
