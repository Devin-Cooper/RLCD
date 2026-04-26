#pragma once
#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

class SpeakerTestScreen : public Screen {
public:
    explicit SpeakerTestScreen(ScreenContext& ctx);
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
private:
    ScreenContext& ctx_;
    int field_ = 0;             // 0=Tone, 1=Length, 2=Volume
    int tone_idx_ = 2;          // 1000 Hz
    int length_idx_ = 1;        // 1 s
    int volume_idx_ = 1;        // Mid
};

}  // namespace app
