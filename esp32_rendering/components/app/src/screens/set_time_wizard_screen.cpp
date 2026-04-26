#include "screens/set_time_wizard_screen.hpp"
#include "screens/set_time_screen.hpp"
#include "time_service.hpp"
#include "screen_stack.hpp"
#include <1bit/render/primitives.hpp>
#include <memory>

namespace app {

SetTimeWizardScreen::SetTimeWizardScreen(ScreenContext& ctx) : ctx_(ctx) {}

void SetTimeWizardScreen::render(onebit::IFramebuffer& fb,
                                 const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    onebit::drawBitmapText(fb, font, 10, 30,  "Clock not set",          onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 60,  "Connect WiFi to sync,",  onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 75,  "or set the time",        onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 90,  "manually.",              onebit::BLACK);

    const char* opt0 = (selected_ == 0) ? ">[Wait for WiFi]" : " [Wait for WiFi]";
    const char* opt1 = (selected_ == 1) ? ">[Set manually]"  : " [Set manually]";
    onebit::drawBitmapText(fb, font, 10, 140, opt0, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 10, 160, opt1, onebit::BLACK);
}

void SetTimeWizardScreen::handleInput(const input::InputEvent& evt,
                                      ScreenStack& stack) {
    using ET = input::EventType;

    // Auto-dismiss: if the wall clock has become valid (e.g. SNTP fired
    // while we were waiting), pop on the next input event of any kind.
    // We can't pop from render() — stack mutations during render trigger
    // an assert — so this is the next best place. WiFi/BLE state-change
    // synthetic events still pass through here, so users typically don't
    // need to press anything.
    if (ctx_.timeService.isTimeValid()) {
        stack.pop();
        return;
    }

    // Buttons (single-button toggle + long-press confirm)
    if (evt.source == input::Source::Button && evt.type == ET::ButtonShort) {
        selected_ ^= 1;
        return;
    }
    if (evt.source == input::Source::Button && evt.type == ET::ButtonLong) {
        if (selected_ == 0) { stack.pop(); return; }
        stack.replace(std::make_unique<SetTimeScreen>(ctx_, /*from_wizard=*/true));
        return;
    }

    // Keyboard
    if (evt.source != input::Source::Keyboard || evt.type != ET::Keypress) return;
    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        if (evt.data[2] == 'A' || evt.data[2] == 'B') { selected_ ^= 1; }
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == '\r') {
        if (selected_ == 0) { stack.pop(); return; }
        stack.replace(std::make_unique<SetTimeScreen>(ctx_, /*from_wizard=*/true));
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
    }
}

}  // namespace app
