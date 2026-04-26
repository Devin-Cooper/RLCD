#pragma once
#include "input_queue.hpp"
#include "ble_hid.hpp"

namespace app {

class OverlayManager;
class ScreenStack;

/// Posts a soft Toast when a bonded BLE keyboard goes offline mid-session.
/// Suppressed on Dashboard and Terminal (primary views — passive viewing
/// or active session, both expected to ignore keyboard state).
class BleSoftToastWatcher {
public:
    BleSoftToastWatcher(OverlayManager& overlay,
                        ScreenStack& stack,
                        ble_hid::BleHidHost& bleHost);

    /// Inspect a synthetic BleStateChanged event after the stack handled it.
    /// No-op for non-BLE events.
    void observe(const input::InputEvent& evt);

private:
    OverlayManager&      overlay_;
    ScreenStack&         stack_;
    ble_hid::BleHidHost& bleHost_;
    ble_hid::State       prev_state_ = ble_hid::State::Disabled;
};

} // namespace app
