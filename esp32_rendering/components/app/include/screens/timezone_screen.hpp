#pragma once

#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

/// POSIX TZ picker: lists the curated `TimeService::catalog()` plus a final
/// "Other (POSIX TZ)..." entry that pushes a `TextInputScreen` for free-form
/// TZ strings.
class TimezoneScreen : public Screen {
public:
    explicit TimezoneScreen(ScreenContext& ctx);

    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    int selected_ = 0;
    int scrollTop_ = 0;
};

}  // namespace app
