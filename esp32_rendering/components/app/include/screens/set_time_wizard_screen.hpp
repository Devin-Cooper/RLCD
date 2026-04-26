#pragma once

#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

/// First-boot wizard shown when `TimeService::wasUnsetAtBoot()` is true.
/// Offers two paths: wait for WiFi/SNTP sync, or set time manually.
/// Auto-dismisses when the system clock becomes valid (e.g. SNTP fires);
/// the auto-dismiss is checked on each input event since Screen has no
/// tick() virtual and stack mutations during render() are forbidden.
class SetTimeWizardScreen : public Screen {
public:
    explicit SetTimeWizardScreen(ScreenContext& ctx);

    bool bypassesKeyboardGate() const override { return true; }
    bool wantsStatusBar() const override { return false; }
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    int selected_ = 0;  // 0 = Wait for WiFi, 1 = Set manually
};

}  // namespace app
