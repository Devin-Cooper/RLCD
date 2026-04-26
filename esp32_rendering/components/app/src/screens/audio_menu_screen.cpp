#include "screens/audio_menu_screen.hpp"
#include "screens/speaker_test_screen.hpp"
#include "screen_stack.hpp"
#include "speaker.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <algorithm>
#include <cstdio>
#include <memory>

namespace app {

AudioMenuScreen::AudioMenuScreen(ScreenContext& ctx) : ctx_(ctx) {}

void AudioMenuScreen::render(onebit::IFramebuffer& fb,
                             const onebit::BitmapFont& font) {
    // Clear screen to white background.
    onebit::fillRect(fb, 0, 0, fb.width(), fb.height(), onebit::WHITE);

    onebit::drawBitmapText(fb, font, 10, 10, "Audio", onebit::BLACK);

    char volline[20];
    std::snprintf(volline, sizeof(volline), "Volume       %u", ctx_.speaker.volume());

    const char* rows[3] = { "Speaker test", "Mic test", volline };
    const int16_t row_h = font.glyph_height + 6;
    for (int i = 0; i < 3; ++i) {
        const int16_t y = 50 + i * row_h;
        if (sel_ == i) {
            onebit::fillRect(fb, 10, y - 2, fb.width() - 20, row_h, onebit::BLACK);
            onebit::drawBitmapText(fb, font, 14, y, rows[i], onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, 14, y, rows[i], onebit::BLACK);
        }
    }
}

void AudioMenuScreen::handleInput(const input::InputEvent& evt,
                                  ScreenStack& stack) {
    using namespace input;
    if (evt.source == Source::Button && evt.type == EventType::ButtonShort && evt.button_id == 1) {
        sel_ = (sel_ + 1) % 3; return;
    }
    if (evt.source == Source::Keyboard && evt.type == EventType::Keypress) {
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            char c = evt.data[2];
            if (c == 'A') { sel_ = (sel_ + 2) % 3; return; }
            if (c == 'B') { sel_ = (sel_ + 1) % 3; return; }
            if (sel_ == 2 && (c == 'C' || c == 'D')) {
                int v = ctx_.speaker.volume();
                v = (c == 'C') ? std::min(100, v + 5) : std::max(0, v - 5);
                ctx_.speaker.setVolume((uint8_t)v);
                return;
            }
        }
        if (evt.data_length == 1 && evt.data[0] == '\r') {
            if (sel_ == 0) stack.push(std::make_unique<SpeakerTestScreen>(ctx_));
            // TODO(Task 15): sel_ == 1 -> push MicTestScreen
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == 0x1B) { stack.pop(); return; }
    }
}

}  // namespace app
