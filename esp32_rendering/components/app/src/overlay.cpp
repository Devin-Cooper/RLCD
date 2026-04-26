#include "overlay.hpp"
#include "screen_context.hpp"
#include "screen_stack.hpp"
#include <1bit/render/primitives.hpp>
#include <cstdio>
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
    modal_.selection = 0;  // default Yes highlighted
    modal_.button_count = 2;
    modal_.active = true;
    modal_.scale_state = ModalScaleState::ScalingIn;
    modal_.scale_complete_us = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 0, 100, kModalScaleUs, now_us_);
}

void OverlayManager::showThreeWay(const char* title, const char* body,
                                  std::array<const char*, 3> labels,
                                  std::function<void(int)> on_result) {
    modal_ = Modal{};
    modal_.kind = ModalKind::ThreeWay;
    std::strncpy(modal_.title, title ? title : "", sizeof(modal_.title) - 1);
    std::strncpy(modal_.body,  body  ? body  : "", sizeof(modal_.body)  - 1);
    modal_.button_count = 3;
    modal_.labels = labels;
    modal_.on_three_way = std::move(on_result);
    modal_.selection = 2;   // default focus = rightmost (Cancel — least destructive)
    modal_.active = true;
    modal_.scale_state = ModalScaleState::ScalingIn;
    modal_.scale_complete_us = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 0, 100, kModalScaleUs, now_us_);
}

// --- Help modal (Phase 9) ---
void OverlayManager::showHelp(const ScreenContext& ctx) {
    if (modal_.active) return;                              // yield to Error/Info/Confirm
    if (help_scale_state_ != HelpScaleState::Hidden) return; // already up

    buildBreadcrumb(ctx.stack, help_breadcrumb_, sizeof(help_breadcrumb_));
    auto* top = ctx.stack.top();
    current_top_hints_ = top ? top->keybindHints()
                             : SpanView<const KeybindHint>{};

    help_scale_state_ = HelpScaleState::ScalingIn;
    help_scale_complete_us_ = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 0, 100, kModalScaleUs, now_us_);
}

void OverlayManager::hideHelp() {
    if (help_scale_state_ != HelpScaleState::ScalingIn
        && help_scale_state_ != HelpScaleState::Visible) return;
    help_scale_state_ = HelpScaleState::ScalingOut;
    help_scale_complete_us_ = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 100, 0, kModalScaleUs, now_us_);
}

#ifdef RLCD_HOST_TEST
void OverlayManager::showHelpForTest(const char* breadcrumb,
                                     SpanView<const KeybindHint> hints) {
    if (modal_.active) return;
    if (help_scale_state_ != HelpScaleState::Hidden) return;
    if (breadcrumb) {
        std::strncpy(help_breadcrumb_, breadcrumb,
                     sizeof(help_breadcrumb_) - 1);
        help_breadcrumb_[sizeof(help_breadcrumb_) - 1] = '\0';
    } else {
        help_breadcrumb_[0] = '\0';
    }
    current_top_hints_ = hints;
    help_scale_state_ = HelpScaleState::ScalingIn;
    help_scale_complete_us_ = now_us_ + static_cast<int64_t>(kModalScaleUs);
    auto tag = makeTag(TweenKind::ModalScale, 0);
    animator_.start(tag, 0, 100, kModalScaleUs, now_us_);
}
#endif

bool OverlayManager::handleInput(const input::InputEvent& evt) {
    // Help modal: any input (button or keypress) dismisses. Modals take
    // precedence — showHelp's guard prevents help opening over a modal,
    // so help and modal are never both up simultaneously.
    if (help_scale_state_ != HelpScaleState::Hidden
        && help_scale_state_ != HelpScaleState::ScalingOut) {
        // Only react to "real" input — ignore button release/none events.
        if (evt.source == input::Source::Keyboard ||
            (evt.source == input::Source::Button &&
             (evt.type == input::EventType::ButtonShort ||
              evt.type == input::EventType::ButtonLong))) {
            hideHelp();
            return true;  // consume; the same input doesn't fall through
        }
        return true;  // help up — swallow other event types too
    }
    if (help_scale_state_ == HelpScaleState::ScalingOut) {
        // Tween settling — swallow to prevent double-trigger flicker.
        return true;
    }

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

    // Confirm / ThreeWay: Left/Right moves selection, Enter selects, Esc = last button.
    const bool is_three_way = (modal_.kind == ModalKind::ThreeWay);
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            if (evt.data[2] == 'C') {
                if (modal_.selection + 1 < modal_.button_count) ++modal_.selection;
            }
            if (evt.data[2] == 'D') {
                if (modal_.selection > 0) --modal_.selection;
            }
        } else if (evt.data_length == 1 && (evt.data[0] == '\r' || evt.data[0] == '\n')) {
            if (is_three_way) {
                int choice = modal_.selection;
                auto cb = std::move(modal_.on_three_way);
                beginDismiss();
                if (cb) cb(choice);
            } else {
                bool yes = (modal_.selection == 0);
                auto cb = std::move(modal_.confirm_cb);
                beginDismiss();
                if (cb) cb(yes);
            }
        } else if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            if (is_three_way) {
                auto cb = std::move(modal_.on_three_way);
                beginDismiss();
                if (cb) cb(modal_.button_count - 1);  // Esc → rightmost (Cancel)
            } else {
                auto cb = std::move(modal_.confirm_cb);
                beginDismiss();
                if (cb) cb(false);
            }
        }
    } else if (evt.source == input::Source::Button &&
               evt.type == input::EventType::ButtonShort) {
        // Button A = advance selection (cycles), Button B = confirm
        if (evt.button_id == 0) {
            if (is_three_way) {
                modal_.selection = (modal_.selection + 1) % modal_.button_count;
            } else {
                modal_.selection = 1 - modal_.selection;
            }
        } else if (evt.button_id == 1) {
            if (is_three_way) {
                int choice = modal_.selection;
                auto cb = std::move(modal_.on_three_way);
                beginDismiss();
                if (cb) cb(choice);
            } else {
                bool yes = (modal_.selection == 0);
                auto cb = std::move(modal_.confirm_cb);
                beginDismiss();
                if (cb) cb(yes);
            }
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

    // Help-modal scale-state advancement (mirrors Modal's lifecycle).
    if (help_scale_state_ == HelpScaleState::ScalingIn
        && now_us >= help_scale_complete_us_) {
        help_scale_state_ = HelpScaleState::Visible;
    }
    if (help_scale_state_ == HelpScaleState::ScalingOut
        && now_us >= help_scale_complete_us_) {
        help_scale_state_ = HelpScaleState::Hidden;
        current_top_hints_ = {};
        help_breadcrumb_[0] = '\0';
    }
}

void OverlayManager::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    if (toast_count_ == 0 && !modal_.active
        && help_scale_state_ == HelpScaleState::Hidden) return;

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

            if (modal_.kind == ModalKind::Confirm
                || modal_.kind == ModalKind::ThreeWay) {
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
                if (modal_.kind == ModalKind::Confirm) {
                    const char* yes = "[ Yes ]";
                    const char* no  = "[ No ]";
                    int16_t no_w  = onebit::getBitmapTextWidth(font, no);
                    int16_t yes_x = mx + 40;
                    int16_t no_x  = mx + mw - 40 - no_w;
                    drawOption(yes_x, yes, modal_.selection == 0);
                    drawOption(no_x,  no,  modal_.selection == 1);
                } else {
                    // ThreeWay: render N buttons evenly across modal width.
                    char bracketed[3][32];
                    int16_t widths[3] = {0, 0, 0};
                    int16_t total_w = 0;
                    for (int i = 0; i < modal_.button_count && i < 3; ++i) {
                        const char* lbl = modal_.labels[i] ? modal_.labels[i] : "";
                        std::snprintf(bracketed[i], sizeof(bracketed[i]),
                                      "[ %s ]", lbl);
                        widths[i] = onebit::getBitmapTextWidth(font, bracketed[i]);
                        total_w += widths[i];
                    }
                    // Distribute remaining horizontal space as gaps.
                    int16_t avail = mw - 16;  // 8 px margin each side
                    int16_t gap = (modal_.button_count > 1)
                        ? (avail - total_w) / (modal_.button_count - 1)
                        : 0;
                    if (gap < 4) gap = 4;
                    int16_t cx = mx + 8;
                    for (int i = 0; i < modal_.button_count && i < 3; ++i) {
                        drawOption(cx, bracketed[i], modal_.selection == i);
                        cx += widths[i] + gap;
                    }
                }
            } else {
                const char* hint = "[ Press any key ]";
                int16_t hw = onebit::getBitmapTextWidth(font, hint);
                onebit::drawBitmapText(fb, font, mx + (mw - hw)/2,
                                       my + mh - font.glyph_height - 10,
                                       hint, onebit::BLACK);
            }
        }
    }

    // Help modal (Phase 9). Shares the ModalScale tween tag with the regular
    // modal — safe because showHelp's guard ensures only one is active at a
    // time. Drawn AFTER toasts/modal so its panel sits on top.
    if (help_scale_state_ != HelpScaleState::Hidden && !modal_.active) {
        auto tag = makeTag(TweenKind::ModalScale, 0);
        int16_t scale = animator_.value(tag, now_us_);
        if (scale > 0) renderHelpModal(fb, font, scale);
    }
}

void OverlayManager::renderHelpModal(onebit::IFramebuffer& fb,
                                     const onebit::BitmapFont& font,
                                     int16_t scale) {
    // Full-size dimensions tuned for 384x168 panel — leave a small frame.
    int16_t panel_w = fb.width();
    int16_t panel_h = fb.height();
    int16_t full_w = (panel_w * 11) / 12;
    int16_t full_h = (panel_h * 11) / 12;
    int16_t cur_w = static_cast<int16_t>((full_w * scale) / 100);
    int16_t cur_h = static_cast<int16_t>((full_h * scale) / 100);
    int16_t x = (panel_w - cur_w) / 2;
    int16_t y = (panel_h - cur_h) / 2;

    onebit::fillRect(fb, x, y, cur_w, cur_h, onebit::WHITE);
    onebit::drawRect(fb, x, y, cur_w, cur_h, onebit::BLACK);
    if (scale < 80) return;  // scale-in still in progress: skip text

    int16_t row_y = y + 4;
    onebit::drawBitmapText(fb, font, x + 6, row_y, "Help", onebit::BLACK);
    row_y += font.glyph_height + 3;
    onebit::fillRect(fb, x + 4, row_y, cur_w - 8, 1, onebit::BLACK);
    row_y += 3;

    onebit::drawBitmapText(fb, font, x + 6, row_y,
                           "Where you are:", onebit::BLACK);
    row_y += font.glyph_height + 1;
    onebit::drawBitmapText(fb, font, x + 12, row_y,
                           help_breadcrumb_, onebit::BLACK);
    row_y += font.glyph_height + 4;

    onebit::drawBitmapText(fb, font, x + 6, row_y,
                           "Keys on this screen:", onebit::BLACK);
    row_y += font.glyph_height + 1;
    // Reserve 4 lines worth of space at the bottom for the always-on chord
    // cheatsheet so per-screen hints don't clobber it.
    int16_t cheat_block_h = (font.glyph_height + 1) * 4 + 4;
    int16_t per_screen_max_y = y + cur_h - cheat_block_h - 4;
    for (size_t i = 0; i < current_top_hints_.size(); ++i) {
        const auto& h = current_top_hints_[i];
        char line[40];
        std::snprintf(line, sizeof(line), "%-10s %s", h.key, h.label);
        if (row_y + font.glyph_height > per_screen_max_y) break;
        onebit::drawBitmapText(fb, font, x + 12, row_y, line, onebit::BLACK);
        row_y += font.glyph_height + 1;
    }

    // Always-on chord cheatsheet at the bottom.
    int16_t cheat_y = y + cur_h - cheat_block_h;
    onebit::fillRect(fb, x + 4, cheat_y, cur_w - 8, 1, onebit::BLACK);
    cheat_y += 2;
    onebit::drawBitmapText(fb, font, x + 6, cheat_y,
                           "Always available:", onebit::BLACK);
    cheat_y += font.glyph_height + 1;
    onebit::drawBitmapText(fb, font, x + 12, cheat_y,
                           "Ctrl+K     command palette", onebit::BLACK);
    cheat_y += font.glyph_height + 1;
    onebit::drawBitmapText(fb, font, x + 12, cheat_y,
                           "Ctrl+/     this help", onebit::BLACK);
    cheat_y += font.glyph_height + 1;
    onebit::drawBitmapText(fb, font, x + 12, cheat_y,
                           "Btn A long pair keyboard", onebit::BLACK);
}

void OverlayManager::renderFooter(onebit::IFramebuffer& fb,
                                  const onebit::BitmapFont& font,
                                  const Screen* top,
                                  int64_t /*now_us*/) {
    if (!top) return;
    if (!top->wantsKeybindFooter()) return;
    if (help_scale_state_ != HelpScaleState::Hidden) return;
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
