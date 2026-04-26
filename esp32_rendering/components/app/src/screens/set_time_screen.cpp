// Stub implementation — Task 9 created this so SetTimeWizardScreen could
// reference SetTimeScreen. Task 10 replaces this body with the real spinner.
#include "screens/set_time_screen.hpp"

namespace app {

SetTimeScreen::SetTimeScreen(ScreenContext& ctx, bool from_wizard)
    : ctx_(ctx), from_wizard_(from_wizard) {}

void SetTimeScreen::clampDay() {}
void SetTimeScreen::adjust(int /*delta*/) {}

void SetTimeScreen::handleInput(const input::InputEvent& /*evt*/,
                                ScreenStack& /*stack*/) {}

void SetTimeScreen::render(onebit::IFramebuffer& /*fb*/,
                           const onebit::BitmapFont& /*font*/) {}

}  // namespace app
