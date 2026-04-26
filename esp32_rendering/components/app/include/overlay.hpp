#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "input_queue.hpp"
#include "animator.hpp"
#include <array>
#include <cstdint>
#include <functional>

namespace app {

enum class ModalKind : uint8_t { Info, Error, Confirm };
enum class ToastSlideState : uint8_t { SlidingIn, Visible, SlidingOut };
enum class ModalScaleState : uint8_t { ScalingIn, Visible, ScalingOut };

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

    // Testing seams
    int activeToastCount() const { return toast_count_; }
    bool hasModal() const { return modal_.active; }

    // Help-modal slot (Phase 9 fills in showHelp, hideHelp, renderHelpModal)
    bool isHelpVisible() const { return help_visible_; }

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
    bool help_visible_ = false;
};

} // namespace app
