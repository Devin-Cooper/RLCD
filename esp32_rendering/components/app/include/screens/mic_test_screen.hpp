#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "microphone.hpp"

namespace app {

class MicTestScreen : public Screen {
public:
    explicit MicTestScreen(ScreenContext& ctx);
    ~MicTestScreen() override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
private:
    ScreenContext& ctx_;
    audio::Microphone::Handle mic_handle_;
    float l_dB_ = -120.0f;
    float r_dB_ = -120.0f;
    bool gain_mode_ = false;
    int gain_channel_ = 0;
};

}  // namespace app
