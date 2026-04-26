#include "screens/mic_test_screen.hpp"
#include "screen_stack.hpp"
#include "audio_helpers.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>

namespace app {

namespace {
int barWidth(float dB, int max_px) {
    float n = (dB + 60.0f) / 60.0f;
    if (n < 0) n = 0;
    if (n > 1) n = 1;
    return (int)(n * (float)max_px);
}
}  // namespace

MicTestScreen::MicTestScreen(ScreenContext& ctx)
    : ctx_(ctx), mic_handle_(ctx.microphone.open()) {}

MicTestScreen::~MicTestScreen() = default;

void MicTestScreen::render(onebit::IFramebuffer& fb,
                           const onebit::BitmapFont& font) {
    // Poll mic non-blocking — this is the tick path (no Screen::tick exists).
    constexpr size_t kFrames = 480;
    static int16_t buf[kFrames * 2];
    if (mic_handle_.valid() &&
        ctx_.microphone.capture(buf, kFrames, pdMS_TO_TICKS(20))) {
        l_dB_ = audio::vuLevelDb(buf, kFrames, 0);
        r_dB_ = audio::vuLevelDb(buf, kFrames, 1);
    }

    onebit::fillRect(fb, 0, 0, fb.width(), fb.height(), onebit::WHITE);
    onebit::drawBitmapText(fb, font, 10, 10, "Mic test", onebit::BLACK);

    const int kMaxBar = fb.width() - 110;
    char buf_l[16];

    onebit::drawBitmapText(fb, font, 10, 50, "L:", onebit::BLACK);
    onebit::fillRect(fb, 30, 50, barWidth(l_dB_, kMaxBar), 8, onebit::BLACK);
    std::snprintf(buf_l, sizeof(buf_l), "%6.1f dB", (double)l_dB_);
    onebit::drawBitmapText(fb, font, fb.width() - 80, 50, buf_l, onebit::BLACK);

    onebit::drawBitmapText(fb, font, 10, 70, "R:", onebit::BLACK);
    onebit::fillRect(fb, 30, 70, barWidth(r_dB_, kMaxBar), 8, onebit::BLACK);
    std::snprintf(buf_l, sizeof(buf_l), "%6.1f dB", (double)r_dB_);
    onebit::drawBitmapText(fb, font, fb.width() - 80, 70, buf_l, onebit::BLACK);

    char gl[24], gr[24];
    std::snprintf(gl, sizeof(gl), "Gain L: %+d dB", ctx_.microphone.gainDb(0));
    std::snprintf(gr, sizeof(gr), "Gain R: %+d dB", ctx_.microphone.gainDb(1));

    auto drawGain = [&](int16_t x, const char* text, bool selected) {
        const int16_t row_h = font.glyph_height + 4;
        if (selected) {
            onebit::fillRect(fb, x - 2, 118, 150, row_h, onebit::BLACK);
            onebit::drawBitmapText(fb, font, x, 120, text, onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, x, 120, text, onebit::BLACK);
        }
    };
    drawGain(10,  gl, gain_mode_ && gain_channel_ == 0);
    drawGain(200, gr, gain_mode_ && gain_channel_ == 1);

    onebit::drawBitmapText(fb, font, 10, 220,
        gain_mode_ ? "[gain] up/dn adj  </> chan  Esc back"
                   : "[Esc] [B-long] gain mode",
        onebit::BLACK);
}

void MicTestScreen::handleInput(const input::InputEvent& evt,
                                ScreenStack& stack) {
    using namespace input;
    if (!gain_mode_) {
        if (evt.source == Source::Button && evt.type == EventType::ButtonLong &&
            evt.button_id == 1) {
            gain_mode_ = true;
            return;
        }
        if (evt.source == Source::Keyboard && evt.type == EventType::Keypress &&
            evt.data_length == 1 && evt.data[0] == 0x1B) {
            stack.pop();
            return;
        }
        return;
    }

    // gain_mode_ == true
    if (evt.source == Source::Keyboard && evt.type == EventType::Keypress) {
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            char c = evt.data[2];
            if (c == 'A') {
                int8_t g = ctx_.microphone.gainDb(gain_channel_);
                if (g < 30) ctx_.microphone.setGainDb(gain_channel_, g + 1);
                return;
            }
            if (c == 'B') {
                int8_t g = ctx_.microphone.gainDb(gain_channel_);
                if (g > -6) ctx_.microphone.setGainDb(gain_channel_, g - 1);
                return;
            }
            if (c == 'C' || c == 'D') {
                gain_channel_ ^= 1;
                return;
            }
        }
        if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            gain_mode_ = false;
            return;
        }
    }
}

}  // namespace app
