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

// Modal methods — stubbed in Task 4; implemented in Task 5.
void OverlayManager::showInfo(const char*, const char*) {}
void OverlayManager::showError(const char*, const char*) {}
void OverlayManager::showConfirm(const char*, const char*,
                                 std::function<void(bool)>) {}

bool OverlayManager::handleInput(const input::InputEvent&) { return false; }

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
}

} // namespace app
