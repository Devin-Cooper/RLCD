#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "input_queue.hpp"
#include "animator.hpp"
#include "screen.hpp"
#include "span_view.hpp"
#include <array>
#include <cstdint>
#include <functional>

namespace app {

// Forward declaration — full include would create a header cycle since
// screen_context.hpp forward-declares OverlayManager.
struct ScreenContext;

enum class ModalKind : uint8_t { Info, Error, Confirm };
enum class ToastSlideState : uint8_t { SlidingIn, Visible, SlidingOut };
enum class ModalScaleState : uint8_t { ScalingIn, Visible, ScalingOut };
enum class HelpScaleState  : uint8_t { Hidden, ScalingIn, Visible, ScalingOut };

class OverlayManager {
public:
    explicit OverlayManager(Animator& animator);

    // --- Toasts (non-blocking) ---
    /// Enqueue a toast. Returns true if queued, false if dropped.
    bool showToast(const char* msg, uint32_t ms = 2500);
    uint32_t droppedToastCount() const { return dropped_toast_count_; }

    // --- Modals (blocking input) — implemented in Task 5 ---
    void showInfo(const char* title, const char* body);
    void showError(const char* title, const char* body);
    void showConfirm(const char* title, const char* body,
                     std::function<void(bool yes)> on_result);

    // --- Per-frame hooks ---
    bool handleInput(const input::InputEvent& evt);
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);
    void tick(int64_t now_us);

    // Render the bottom-of-screen 12 px keybind footer for `top` if it wants one.
    // Suppressed when wantsKeybindFooter() == false, when help modal is visible,
    // when a blocking modal (Error/Info/Confirm) is active, or when any toast
    // is in flight (toasts overlap the footer y-range).
    void renderFooter(onebit::IFramebuffer& fb,
                      const onebit::BitmapFont& font,
                      const Screen* top,
                      int64_t now_us);

    // Testing seams
    int activeToastCount() const { return toast_count_; }
    bool hasModal() const { return modal_.active; }

    // --- Help modal (Phase 9) ---
    /// Open the help modal: captures breadcrumb + top screen's keybind hints.
    /// No-op when an Error/Info/Confirm modal is active or help is already up.
    void showHelp(const ScreenContext& ctx);
    /// Begin scale-out. Idempotent on Hidden/ScalingOut.
    void hideHelp();
    /// True whenever the help modal is in any non-Hidden state (including
    /// the scale-in and scale-out windows).
    bool isHelpVisible() const {
        return help_scale_state_ != HelpScaleState::Hidden;
    }

#ifdef RLCD_HOST_TEST
    // Host-test seam — bypasses ScreenContext (which references many ESP-IDF
    // backends that have no host stand-in). Mirrors showHelp's body.
    void showHelpForTest(const char* breadcrumb,
                         SpanView<const KeybindHint> hints);
#endif

private:
    static constexpr int MAX_TOASTS = 3;
    static constexpr int TOAST_MSG_MAX = 64;

    struct Toast {
        char msg[TOAST_MSG_MAX];
        int64_t expires_us;
        ToastSlideState slide_state = ToastSlideState::SlidingIn;
        int64_t slide_complete_us = 0;
    };
    std::array<Toast, MAX_TOASTS> toasts_{};
    int toast_count_ = 0;
    uint32_t dropped_toast_count_ = 0;

    char last_toast_msg_[TOAST_MSG_MAX] = {};
    int64_t last_toast_enqueued_us_ = 0;

    struct Modal {
        ModalKind kind = ModalKind::Info;
        char title[32] = {};
        char body[128] = {};
        std::function<void(bool)> confirm_cb;
        bool active = false;
        int confirm_selection = 0;  // 0 = Yes, 1 = No
        ModalScaleState scale_state = ModalScaleState::ScalingIn;
        int64_t scale_complete_us = 0;
    };
    Modal modal_;

    Animator& animator_;
    int64_t now_us_ = 0;

    // Help-modal state. The scale state lifecycle mirrors Modal's: ScalingIn
    // → Visible → ScalingOut → Hidden, advanced by tick(). Captured at
    // show-time so the help text doesn't change if the underlying stack
    // mutates (e.g. background System events) while help is open.
    HelpScaleState help_scale_state_ = HelpScaleState::Hidden;
    int64_t help_scale_complete_us_ = 0;
    char help_breadcrumb_[128] = {};
    SpanView<const KeybindHint> current_top_hints_{};

    void renderHelpModal(onebit::IFramebuffer& fb,
                         const onebit::BitmapFont& font,
                         int16_t scale);
};

} // namespace app
