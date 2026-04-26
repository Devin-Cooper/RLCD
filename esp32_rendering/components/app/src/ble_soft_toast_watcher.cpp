#include "ble_soft_toast_watcher.hpp"
#include "overlay.hpp"
#include "screen_stack.hpp"
#include "screen.hpp"

namespace app {

BleSoftToastWatcher::BleSoftToastWatcher(OverlayManager& overlay,
                                          ScreenStack& stack,
                                          ble_hid::BleHidHost& bleHost)
    : overlay_(overlay), stack_(stack), bleHost_(bleHost) {}

void BleSoftToastWatcher::observe(const input::InputEvent& evt) {
    if (evt.source != input::Source::System) return;
    if (evt.type != input::EventType::BleStateChanged) return;
    if (evt.data_length < 1) return;

    auto new_state = static_cast<ble_hid::State>(evt.data[0]);
    bool was_active = (prev_state_ == ble_hid::State::Connected
                    || prev_state_ == ble_hid::State::Connecting);
    bool now_disconnected = (new_state == ble_hid::State::Disconnected);
    prev_state_ = new_state;

    if (!was_active || !now_disconnected) return;
    if (!bleHost_.hasBond()) return;

    auto* top = stack_.top();
    if (!top) return;
    auto kind = top->screenKind();
    if (kind == ScreenKind::Dashboard) return;
    if (kind == ScreenKind::Terminal) return;

    overlay_.showToast("Keyboard offline - long-press A to re-pair");
}

} // namespace app
