#include "screens/speaker_test_screen.hpp"
#include "screen_stack.hpp"
#include "speaker.hpp"
#include "audio_helpers.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <cstdio>

namespace app {

namespace {
constexpr uint32_t kFreqs[]    = {250, 500, 1000, 2000, 4000};
constexpr uint32_t kLensMs[]   = {500, 1000, 2000, 5000};
constexpr uint8_t  kVols[]     = {25, 60, 90};
constexpr const char* kVolNames[] = {"Lo", "Mid", "Hi"};
constexpr int N_TONE = sizeof(kFreqs)  / sizeof(kFreqs[0]);
constexpr int N_LEN  = sizeof(kLensMs) / sizeof(kLensMs[0]);
constexpr int N_VOL  = sizeof(kVols)   / sizeof(kVols[0]);

void drawRow(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
             int16_t y, const char* text, bool selected) {
    const int16_t row_h = font.glyph_height + 6;
    if (selected) {
        onebit::fillRect(fb, 10, y - 2, fb.width() - 20, row_h, onebit::BLACK);
        onebit::drawBitmapText(fb, font, 14, y, text, onebit::WHITE);
    } else {
        onebit::drawBitmapText(fb, font, 14, y, text, onebit::BLACK);
    }
}
}  // namespace

SpeakerTestScreen::SpeakerTestScreen(ScreenContext& ctx) : ctx_(ctx) {}

void SpeakerTestScreen::render(onebit::IFramebuffer& fb,
                               const onebit::BitmapFont& font) {
    onebit::fillRect(fb, 0, 0, fb.width(), fb.height(), onebit::WHITE);
    onebit::drawBitmapText(fb, font, 10, 10, "Speaker test", onebit::BLACK);

    char l[40];
    const int16_t row_h = font.glyph_height + 6;

    std::snprintf(l, sizeof(l), "Tone:    %lu Hz", (unsigned long)kFreqs[tone_idx_]);
    drawRow(fb, font, 50, l, field_ == 0);

    std::snprintf(l, sizeof(l), "Length:  %lu ms", (unsigned long)kLensMs[length_idx_]);
    drawRow(fb, font, 50 + row_h, l, field_ == 1);

    std::snprintf(l, sizeof(l), "Volume:  %s", kVolNames[volume_idx_]);
    drawRow(fb, font, 50 + 2 * row_h, l, field_ == 2);

    onebit::drawBitmapText(fb, font, 14, 200, "[Play] Btn-A / Enter", onebit::BLACK);
    onebit::drawBitmapText(fb, font, 14, 215, "[Esc]", onebit::BLACK);
}

void SpeakerTestScreen::handleInput(const input::InputEvent& evt,
                                    ScreenStack& stack) {
    using namespace input;
    if (evt.source == Source::Button && evt.type == EventType::ButtonShort) {
        if (evt.button_id == 0) {
            audio::beep(ctx_.speaker, kFreqs[tone_idx_], kLensMs[length_idx_], kVols[volume_idx_]);
            return;
        }
        if (evt.button_id == 1) {
            field_ = (field_ + 1) % 3;
            return;
        }
    }
    if (evt.source == Source::Keyboard && evt.type == EventType::Keypress) {
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            char c = evt.data[2];
            if (c == 'A') { field_ = (field_ + 2) % 3; return; }
            if (c == 'B') { field_ = (field_ + 1) % 3; return; }
            if (c == 'D') {
                if (field_ == 0) tone_idx_   = (tone_idx_   + N_TONE - 1) % N_TONE;
                else if (field_ == 1) length_idx_ = (length_idx_ + N_LEN  - 1) % N_LEN;
                else volume_idx_ = (volume_idx_ + N_VOL  - 1) % N_VOL;
                return;
            }
            if (c == 'C') {
                if (field_ == 0) tone_idx_   = (tone_idx_   + 1) % N_TONE;
                else if (field_ == 1) length_idx_ = (length_idx_ + 1) % N_LEN;
                else volume_idx_ = (volume_idx_ + 1) % N_VOL;
                return;
            }
        }
        if (evt.data_length == 1 && evt.data[0] == '\r') {
            audio::beep(ctx_.speaker, kFreqs[tone_idx_], kLensMs[length_idx_], kVols[volume_idx_]);
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            stack.pop();
            return;
        }
    }
}

}  // namespace app
