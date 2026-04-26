#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "pcf85063.hpp"

namespace app {

/// Manual Y/M/D/H/M/S spinner for setting the wall clock.
///
/// `from_wizard` is true when reached from `SetTimeWizardScreen` (first-boot
/// keyboard-gate bypass). When set, `bypassesKeyboardGate()` returns true so
/// the user can complete first-boot setup without a paired BLE keyboard.
class SetTimeScreen : public Screen {
public:
    SetTimeScreen(ScreenContext& ctx, bool from_wizard);

    bool bypassesKeyboardGate() const override { return from_wizard_; }
    bool wantsStatusBar() const override { return false; }
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    bool from_wizard_;
    int field_ = 0;  // 0..5 for Y/M/D/H/M/S
    sensors::RtcTime t_{};

    void clampDay();
    void adjust(int delta);
};

}  // namespace app
