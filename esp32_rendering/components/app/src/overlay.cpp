#include "overlay.hpp"
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <esp_log.h>

static const char* TAG = "overlay";

namespace app {

OverlayManager::OverlayManager(Animator& animator) : animator_(animator) {}

bool OverlayManager::showToast(const char* msg, uint32_t ms) {
    const int64_t DEDUP_WINDOW_US = 500000;  // 500ms

    if (!msg) return false;

    // Dedup identical string within the dedup window.
    // Amendment G: only dedup when there was a previous toast — otherwise
    // first boot-time toast is silently dropped against empty state.
    if (last_toast_msg_[0] != '\0' &&
        now_us_ - last_toast_enqueued_us_ < DEDUP_WINDOW_US &&
        std::strncmp(last_toast_msg_, msg, TOAST_MSG_MAX) == 0) {
        dropped_toast_count_++;
        return false;
    }

    if (toast_count_ >= MAX_TOASTS) {
        dropped_toast_count_++;
        ESP_LOGW(TAG, "toast dropped (queue full): '%s'", msg);
        return false;
    }

    int idx = toast_count_++;
    Toast& t = toasts_[idx];
    std::strncpy(t.msg, msg, TOAST_MSG_MAX - 1);
    t.msg[TOAST_MSG_MAX - 1] = '\0';
    t.expires_us = now_us_ + static_cast<int64_t>(ms) * 1000;
    t.slide_state = ToastSlideState::SlidingIn;
    t.slide_complete_us = now_us_ + static_cast<int64_t>(kToastSlideUs);

    auto tag = makeTag(TweenKind::ToastSlide, static_cast<uint32_t>(idx));
    animator_.start(tag, /*from=*/16, /*to=*/0, kToastSlideUs, now_us_);

    std::strncpy(last_toast_msg_, msg, TOAST_MSG_MAX - 1);
    last_toast_msg_[TOAST_MSG_MAX - 1] = '\0';
    last_toast_enqueued_us_ = now_us_;
    return true;
}

// Modal methods — implemented in Task 5.
void OverlayManager::showInfo(const char* title, const char* body) {
    modal_ = Modal{};
    modal_.kind = ModalKind::Info;
    std::strncpy(modal_.title, title ? title : "", sizeof(modal_.title) - 1);
    std::strncpy(modal_.body,  body  ? body  : "", sizeof(modal_.body)  - 1);
    modal_.active = true;
    modal_.scale_state = ModalScaleState::ScalingIn;
    modal_.scale_complete_us = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 0, 100, kModalScaleUs, now_us_);
}

void OverlayManager::showError(const char* title, const char* body) {
    showInfo(title, body);
    modal_.kind = ModalKind::Error;
}

void OverlayManager::showConfirm(const char* title, const char* body,
                                 std::function<void(bool)> on_result) {
    modal_ = Modal{};
    modal_.kind = ModalKind::Confirm;
    std::strncpy(modal_.title, title ? title : "", sizeof(modal_.title) - 1);
    std::strncpy(modal_.body,  body  ? body  : "", sizeof(modal_.body)  - 1);
    modal_.confirm_cb = std::move(on_result);
    modal_.confirm_selection = 0;  // default Yes highlighted
    modal_.active = true;
    modal_.scale_state = ModalScaleState::ScalingIn;
    modal_.scale_complete_us = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 0, 100, kModalScaleUs, now_us_);
}

bool OverlayManager::handleInput(const input::InputEvent& evt) {
    if (!modal_.active) return false;

    // Helper: kick off scale-out tween. The modal stays "active" until the
    // tween settles in tick(); during that window input is still consumed.
    auto beginDismiss = [&]() {
        modal_.scale_state = ModalScaleState::ScalingOut;
        modal_.scale_complete_us =
            now_us_ + static_cast<int64_t>(kModalScaleUs);
        auto tag = makeTag(TweenKind::ModalScale, 0);
        animator_.start(tag, 100, 0, kModalScaleUs, now_us_);
    };

    // While the modal is already retracting, swallow further input.
    if (modal_.scale_state == ModalScaleState::ScalingOut) {
        return true;
    }

    // Info/Error: any key dismisses.
    if (modal_.kind == ModalKind::Info || modal_.kind == ModalKind::Error) {
        if (evt.source == input::Source::Keyboard ||
            (evt.source == input::Source::Button &&
             (evt.type == input::EventType::ButtonShort ||
              evt.type == input::EventType::ButtonLong))) {
            beginDismiss();
        }
        return true;  // always consume while active
    }

    // Confirm: Left/Right toggles, Enter selects, Esc = No.
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            if (evt.data[2] == 'C') modal_.confirm_selection = 1; // Right → No
            if (evt.data[2] == 'D') modal_.confirm_selection = 0; // Left → Yes
        } else if (evt.data_length == 1 && evt.data[0] == '\r') {
            bool yes = (modal_.confirm_selection == 0);
            auto cb = std::move(modal_.confirm_cb);
            beginDismiss();
            if (cb) cb(yes);
        } else if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            auto cb = std::move(modal_.confirm_cb);
            beginDismiss();
            if (cb) cb(false);
        }
    } else if (evt.source == input::Source::Button &&
               evt.type == input::EventType::ButtonShort) {
        // Button A = toggle, Button B = confirm
        if (evt.button_id == 0) {
            modal_.confirm_selection = 1 - modal_.confirm_selection;
        } else if (evt.button_id == 1) {
            bool yes = (modal_.confirm_selection == 0);
            auto cb = std::move(modal_.confirm_cb);
            beginDismiss();
            if (cb) cb(yes);
        }
    }
    return true;
}

void OverlayManager::tick(int64_t now_us) {
    now_us_ = now_us;

    // Walk toasts, advancing slide state and dropping ones that have
    // finished sliding out.
    for (int i = 0; i < toast_count_; ) {
        Toast& t = toasts_[i];
        // SlidingIn -> Visible once the slide-in tween settles.
        if (t.slide_state == ToastSlideState::SlidingIn
            && now_us >= t.slide_complete_us) {
            t.slide_state = ToastSlideState::Visible;
        }
        // Visible -> SlidingOut kToastSlideUs before expiry.
        int64_t slide_out_start = t.expires_us - static_cast<int64_t>(kToastSlideUs);
        if (t.slide_state == ToastSlideState::Visible
            && now_us >= slide_out_start) {
            auto tag = makeTag(TweenKind::ToastSlide, static_cast<uint32_t>(i));
            animator_.start(tag, 0, 16, kToastSlideUs, slide_out_start);
            t.slide_state = ToastSlideState::SlidingOut;
            t.slide_complete_us = slide_out_start + static_cast<int64_t>(kToastSlideUs);
        }
        // SlidingOut complete -> drop and shift down.
        if (t.slide_state == ToastSlideState::SlidingOut
            && now_us >= t.slide_complete_us) {
            for (int j = i; j < toast_count_ - 1; ++j) toasts_[j] = toasts_[j + 1];
            --toast_count_;
            continue;
        }
        ++i;
    }

    // Modal scale-out completion: deactivate when the retract tween settles.
    if (modal_.active && modal_.scale_state == ModalScaleState::ScalingOut
        && now_us >= modal_.scale_complete_us) {
        modal_.active = false;
    }
}

void OverlayManager::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    if (toast_count_ == 0 && !modal_.active) return;

    const int16_t margin = 4;
    const int16_t pad = 3;
    int16_t y = fb.height() - margin;

    for (int i = toast_count_ - 1; i >= 0; --i) {
        const char* msg = toasts_[i].msg;
        int16_t w = onebit::getBitmapTextWidth(font, msg);
        int16_t box_w = w + pad * 2;
        int16_t box_h = font.glyph_height + pad * 2;
        int16_t box_x = (fb.width() - box_w) / 2;
        auto tag = makeTag(TweenKind::ToastSlide, static_cast<uint32_t>(i));
        int16_t y_offset = animator_.value(tag, now_us_);
        int16_t box_y = y - box_h + y_offset;

        onebit::fillRect(fb, box_x, box_y, box_w, box_h, onebit::WHITE);
        onebit::drawRect(fb, box_x, box_y, box_w, box_h, onebit::BLACK);
        onebit::drawBitmapText(fb, font, box_x + pad, box_y + pad,
                               msg, onebit::BLACK);
        y = (box_y - y_offset) - 2;  // next toast stacks on the un-offset position
    }

    if (modal_.active) {
        const int16_t mw = 300;
        const int16_t mh = 120;

        auto modal_tag = makeTag(TweenKind::ModalScale, 0);
        int16_t scale = animator_.value(modal_tag, now_us_);
        if (scale <= 0) return;  // tween hasn't started or fully retracted

        int16_t cur_w = static_cast<int16_t>((mw * scale) / 100);
        int16_t cur_h = static_cast<int16_t>((mh * scale) / 100);
        int16_t mx_cur = (fb.width() - cur_w) / 2;
        int16_t my_cur = (fb.height() - cur_h) / 2;

        onebit::fillRect(fb, mx_cur, my_cur, cur_w, cur_h, onebit::WHITE);
        onebit::drawRect(fb, mx_cur, my_cur, cur_w, cur_h, onebit::BLACK);

        // Only draw text/inner border when the modal is at (or very near)
        // full size — avoids garbled text during the brief scale tween.
        if (scale > 80) {
            const int16_t mx = (fb.width() - mw) / 2;
            const int16_t my = (fb.height() - mh) / 2;

            onebit::drawRect(fb, mx+2, my+2, mw-4, mh-4, onebit::BLACK);
            onebit::drawBitmapText(fb, font, mx + 8, my + 8,
                                   modal_.title, onebit::BLACK);
            onebit::drawBitmapText(fb, font, mx + 8, my + 28,
                                   modal_.body, onebit::BLACK);

            if (modal_.kind == ModalKind::Confirm) {
                const char* yes = "[ Yes ]";
                const char* no  = "[ No ]";
                int16_t no_w  = onebit::getBitmapTextWidth(font, no);
                int16_t yes_x = mx + 40;
                int16_t no_x  = mx + mw - 40 - no_w;
                int16_t bot_y = my + mh - font.glyph_height - 10;

                auto drawOption = [&](int16_t x, const char* label, bool sel) {
                    if (sel) {
                        onebit::fillRect(fb, x - 2, bot_y - 2,
                                         onebit::getBitmapTextWidth(font, label) + 4,
                                         font.glyph_height + 4, onebit::BLACK);
                        onebit::drawBitmapText(fb, font, x, bot_y,
                                               label, onebit::WHITE);
                    } else {
                        onebit::drawBitmapText(fb, font, x, bot_y,
                                               label, onebit::BLACK);
                    }
                };
                drawOption(yes_x, yes, modal_.confirm_selection == 0);
                drawOption(no_x,  no,  modal_.confirm_selection == 1);
            } else {
                const char* hint = "[ Press any key ]";
                int16_t hw = onebit::getBitmapTextWidth(font, hint);
                onebit::drawBitmapText(fb, font, mx + (mw - hw)/2,
                                       my + mh - font.glyph_height - 10,
                                       hint, onebit::BLACK);
            }
        }
    }
}

void OverlayManager::renderFooter(onebit::IFramebuffer& fb,
                                  const onebit::BitmapFont& font,
                                  const Screen* top,
                                  int64_t /*now_us*/) {
    if (!top) return;
    if (!top->wantsKeybindFooter()) return;
    if (help_visible_) return;
    if (modal_.active) return;
    if (toast_count_ > 0) return;  // toast in flight overlaps footer y-range

    auto hints = top->keybindHints();
    if (hints.empty()) return;

    constexpr int FOOTER_H = 12;
    int16_t panel_w = fb.width();
    int16_t panel_h = fb.height();
    int16_t y = panel_h - FOOTER_H;

    // Hairline above the footer
    onebit::fillRect(fb, 0, y - 1, panel_w, 1, onebit::BLACK);

    // Render hints left-to-right, truncating from the right when out of space.
    int16_t x = 2;
    constexpr int16_t kInnerPad = 2;     // padding inside the inverted "key" pill
    constexpr int16_t kGroupGap = 6;     // gap between (pill + label) groups
    constexpr int16_t kKeyLabelGap = 3;  // gap between pill and its label

    for (size_t i = 0; i < hints.size(); ++i) {
        const auto& h = hints[i];
        int16_t key_text_w = onebit::getBitmapTextWidth(font, h.key);
        int16_t key_pill_w = key_text_w + 2 * kInnerPad;
        int16_t label_w = onebit::getBitmapTextWidth(font, h.label);
        int16_t group_w = key_pill_w + kKeyLabelGap + label_w;

        // Truncate if this group won't fit
        if (x + group_w > panel_w - 2) break;

        // Inverted pill: black box, white text
        int16_t pill_y = y + 1;
        int16_t pill_h = FOOTER_H - 2;
        onebit::fillRect(fb, x, pill_y, key_pill_w, pill_h, onebit::BLACK);
        // Vertically center the text in the pill
        int16_t text_y = pill_y + (pill_h - font.glyph_height) / 2;
        onebit::drawBitmapText(fb, font, x + kInnerPad, text_y, h.key, onebit::WHITE);

        // Label in normal black-on-white
        onebit::drawBitmapText(fb, font, x + key_pill_w + kKeyLabelGap, text_y,
                               h.label, onebit::BLACK);

        x += group_w + kGroupGap;
    }
}

} // namespace app
