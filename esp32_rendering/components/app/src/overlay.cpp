#include "overlay.hpp"
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <esp_log.h>

static const char* TAG = "overlay";

namespace app {

OverlayManager::OverlayManager() = default;

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

    Toast& t = toasts_[toast_count_++];
    std::strncpy(t.msg, msg, TOAST_MSG_MAX - 1);
    t.msg[TOAST_MSG_MAX - 1] = '\0';
    t.expires_us = now_us_ + static_cast<int64_t>(ms) * 1000;

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
}

bool OverlayManager::handleInput(const input::InputEvent& evt) {
    if (!modal_.active) return false;

    // Info/Error: any key dismisses.
    if (modal_.kind == ModalKind::Info || modal_.kind == ModalKind::Error) {
        if (evt.source == input::Source::Keyboard ||
            (evt.source == input::Source::Button &&
             (evt.type == input::EventType::ButtonShort ||
              evt.type == input::EventType::ButtonLong))) {
            modal_.active = false;
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
            modal_.active = false;
            if (cb) cb(yes);
        } else if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            auto cb = std::move(modal_.confirm_cb);
            modal_.active = false;
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
            modal_.active = false;
            if (cb) cb(yes);
        }
    }
    return true;
}

void OverlayManager::tick(int64_t now_us) {
    now_us_ = now_us;

    // Expire toasts — compact in-place.
    int write = 0;
    for (int read = 0; read < toast_count_; ++read) {
        if (toasts_[read].expires_us > now_us_) {
            if (write != read) toasts_[write] = toasts_[read];
            ++write;
        }
    }
    toast_count_ = write;
}

void OverlayManager::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    if (toast_count_ == 0) return;

    const int16_t margin = 4;
    const int16_t pad = 3;
    int16_t y = fb.height() - margin;

    for (int i = toast_count_ - 1; i >= 0; --i) {
        const char* msg = toasts_[i].msg;
        int16_t w = onebit::getBitmapTextWidth(font, msg);
        int16_t box_w = w + pad * 2;
        int16_t box_h = font.glyph_height + pad * 2;
        int16_t box_x = (fb.width() - box_w) / 2;
        int16_t box_y = y - box_h;

        onebit::fillRect(fb, box_x, box_y, box_w, box_h, onebit::WHITE);
        onebit::drawRect(fb, box_x, box_y, box_w, box_h, onebit::BLACK);
        onebit::drawBitmapText(fb, font, box_x + pad, box_y + pad,
                               msg, onebit::BLACK);
        y = box_y - 2;
    }

    if (modal_.active) {
        const int16_t mw = 300;
        const int16_t mh = 120;
        const int16_t mx = (fb.width() - mw) / 2;
        const int16_t my = (fb.height() - mh) / 2;

        onebit::fillRect(fb, mx, my, mw, mh, onebit::WHITE);
        onebit::drawRect(fb, mx, my, mw, mh, onebit::BLACK);
        onebit::drawRect(fb, mx+2, my+2, mw-4, mh-4, onebit::BLACK);

        onebit::drawBitmapText(fb, font, mx + 8, my + 8,
                               modal_.title, onebit::BLACK);
        onebit::drawBitmapText(fb, font, mx + 8, my + 28,
                               modal_.body, onebit::BLACK);

        if (modal_.kind == ModalKind::Confirm) {
            const char* yes = "[ Yes ]";
            const char* no  = "[ No ]";
            int16_t yes_w = onebit::getBitmapTextWidth(font, yes);
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

} // namespace app
