#pragma once
#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

class AudioMenuScreen : public Screen {
public:
    explicit AudioMenuScreen(ScreenContext& ctx);
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
private:
    ScreenContext& ctx_;
    int sel_ = 0;
};

}  // namespace app
