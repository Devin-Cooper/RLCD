#include "screens/wifi_screen.hpp"
#include "screens/password_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <cstdio>
#include <string>
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "wifi_screen";

namespace app {

WifiScreen::WifiScreen(ScreenContext& ctx) : ctx_(ctx) {}

int16_t WifiScreen::computeRowY(int index) const {
    return list_start_y_ + static_cast<int16_t>(index) * row_h_;
}

void WifiScreen::onSelectionChange(int old_index, int new_index) {
    if (old_index == new_index) return;
    if (!focus_y_initialized_) return;
    int16_t old_y = computeRowY(old_index);
    int16_t new_y = computeRowY(new_index);
    auto tag = makeTag(TweenKind::FocusRect, focus_id::WifiScreen);
    ctx_.animator.start(tag, old_y, new_y, kFocusRectUs, esp_timer_get_time());
    prev_selected_y_ = new_y;
}

void WifiScreen::onEnter() {
    wifi::ConnectionInfo ci = ctx_.wifiMgr.connectionInfo();
    tab_ = (ci.state == wifi::State::Connected) ? Tab::Known : Tab::Available;
    refreshKnown();
    onEnterTab(tab_);
}

void WifiScreen::onEnterTab(Tab t) {
    tab_ = t;
    sel_ = 0;
    if (t == Tab::Available && !scan_in_flight_) {
        startScan();
    }
    if (t == Tab::Known) {
        refreshKnown();
    }
    // Tab change: cancel any in-flight focus-rect tween and snap to row 0.
    auto tag = makeTag(TweenKind::FocusRect, focus_id::WifiScreen);
    ctx_.animator.cancel(tag);
    focus_y_initialized_ = false;
}

void WifiScreen::refreshKnown() {
    known_count_ = ctx_.wifiMgr.knownNetworks(
        known_, wifi::WifiManager::MAX_KNOWN_NETWORKS);
}

void WifiScreen::startScan() {
    ctx_.wifiMgr.startScan();
    scan_in_flight_ = true;
    ctx_.overlay.showToast("Scanning...", 1200);
}

void WifiScreen::refreshScanResults() {
    scan_count_ = ctx_.wifiMgr.scanResults(
        scan_, wifi::WifiManager::MAX_SCAN_RESULTS);
    int w = 0;
    for (int r = 0; r < scan_count_; ++r) {
        if (scan_[r].ssid[0] != '\0') {
            if (w != r) scan_[w] = scan_[r];
            ++w;
        }
    }
    scan_count_ = w;
    scan_in_flight_ = false;
    sel_ = 0;
    // Result list refreshed; cancel in-flight tween and re-init y.
    auto tag = makeTag(TweenKind::FocusRect, focus_id::WifiScreen);
    ctx_.animator.cancel(tag);
    focus_y_initialized_ = false;
}

void WifiScreen::sanitize(char* dst, const char* src, size_t dst_cap) {
    size_t i = 0;
    for (; src[i] && i + 1 < dst_cap; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        dst[i] = (c >= 0x20 && c <= 0x7E) ? static_cast<char>(c) : '?';
    }
    dst[i] = '\0';
}

int WifiScreen::visibleCount() const {
    return (tab_ == Tab::Available) ? scan_count_ : known_count_;
}

void WifiScreen::handleInput(const input::InputEvent& evt, ScreenStack& stack) {
    // Amendment H: WifiScanDone (not state change) drives result refresh.
    if (evt.source == input::Source::System &&
        evt.type == input::EventType::WifiScanDone) {
        refreshScanResults();
        return;
    }
    // WifiStateChanged drives connect-success toast (future: Task 17+).
    // For Task 15/16 boundary, Available-tab connect handled inline below.
    if (evt.source == input::Source::System &&
        evt.type == input::EventType::WifiStateChanged) {
        return;
    }

    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            char c = evt.data[2];
            int count = visibleCount();
            int old_sel = sel_;
            if (c == 'A' && count > 0) sel_ = (sel_ - 1 + count) % count;
            if (c == 'B' && count > 0) sel_ = (sel_ + 1) % count;
            if (c == 'A' || c == 'B') {
                onSelectionChange(old_sel, sel_);
                return;
            }
            if (c == 'C') onEnterTab(tab_ == Tab::Known ? Tab::Available : Tab::Known);
            if (c == 'D') onEnterTab(tab_ == Tab::Available ? Tab::Known : Tab::Available);
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == '\r') {
            connectSelected(stack);
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            stack.pop();
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == 'r' && tab_ == Tab::Available) {
            startScan();
            return;
        }
        // Shift+D (capital D only): forget with confirm, Known tab only
        if (evt.data_length == 1 && evt.data[0] == 'D' &&
            tab_ == Tab::Known && sel_ < known_count_) {
            char raw_ssid[33];
            std::strncpy(raw_ssid, known_[sel_].ssid, sizeof(raw_ssid) - 1);
            raw_ssid[sizeof(raw_ssid) - 1] = '\0';

            std::string forgotten_ssid(raw_ssid);

            char body[64];
            snprintf(body, sizeof(body), "Forget %s?", raw_ssid);
            ctx_.overlay.showConfirm("Confirm", body,
                [this, forgotten_ssid](bool yes) {
                    if (!yes) return;
                    ctx_.wifiMgr.forgetNetwork(forgotten_ssid.c_str());
                    refreshKnown();
                    if (sel_ >= known_count_) sel_ = std::max(0, known_count_ - 1);
                    ctx_.overlay.showToast("Forgotten", 1500);
                });
            return;
        }
    }

    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort) {
        if (evt.button_id == 0) {
            stack.pop();
        } else if (evt.button_id == 1) {
            int count = visibleCount();
            if (count > 0) {
                int old_sel = sel_;
                sel_ = (sel_ + 1) % count;
                onSelectionChange(old_sel, sel_);
            }
        }
    }
}

void WifiScreen::connectSelected(ScreenStack& stack) {
    if (tab_ == Tab::Known) {
        if (sel_ < 0 || sel_ >= known_count_) return;
        char pw[65];
        if (ctx_.wifiMgr.knownPassword(known_[sel_].ssid, pw, sizeof(pw))) {
            ctx_.wifiMgr.connect(known_[sel_].ssid, pw);
            ctx_.overlay.showToast("Connecting...", 1500);
        }
        return;
    }
    // Available tab
    if (sel_ < 0 || sel_ >= scan_count_) return;

    const wifi::NetworkInfo& n = scan_[sel_];
    if (n.auth == WIFI_AUTH_OPEN) {
        ctx_.wifiMgr.connect(n.ssid, "");
        ctx_.overlay.showToast("Connecting...", 1500);
    } else {
        stack.push(std::make_unique<PasswordScreen>(ctx_, n.ssid));
    }
}

void WifiScreen::render(onebit::IFramebuffer& fb,
                        const onebit::BitmapFont& font) {
    int16_t y = 4;
    onebit::drawBitmapText(fb, font, 10, y,
        tab_ == Tab::Known ? ">Known<" : " Known ",
        onebit::BLACK);
    onebit::drawBitmapText(fb, font, 120, y,
        tab_ == Tab::Available ? ">Available<" : " Available ",
        onebit::BLACK);
    y += font.glyph_height + 4;
    onebit::fillRect(fb, 10, y, fb.width() - 20, 1, onebit::BLACK);
    y += 4;

    // Cache list layout for the focus-rect animation.
    list_start_y_ = y - 1;                 // top y of focus rect for row 0
    row_h_        = font.glyph_height + 2;

    // Compute the focus-rect y. Animate if a tween is in progress.
    auto tag = makeTag(TweenKind::FocusRect, focus_id::WifiScreen);
    int64_t now = esp_timer_get_time();
    int count = visibleCount();
    if (count > 0 && !focus_y_initialized_) {
        prev_selected_y_ = computeRowY(sel_);
        focus_y_initialized_ = true;
    }
    int16_t cur_y = ctx_.animator.inProgress(tag, now)
                  ? ctx_.animator.value(tag, now)
                  : prev_selected_y_;

    if (tab_ == Tab::Available) {
        if (scan_in_flight_) {
            onebit::drawBitmapText(fb, font, 10, y, "Scanning...", onebit::BLACK);
            return;
        }
        if (scan_count_ == 0) {
            onebit::drawBitmapText(fb, font, 10, y,
                                   "No networks. 'r' to rescan.", onebit::BLACK);
            return;
        }
        // Draw the focus rect at the (possibly interpolated) y.
        onebit::fillRect(fb, 8, cur_y, fb.width() - 16,
                         font.glyph_height + 2, onebit::BLACK);
        for (int i = 0; i < scan_count_ && y + font.glyph_height < fb.height() - 20; ++i) {
            char disp[48];
            sanitize(disp, scan_[i].ssid, sizeof(disp));

            char line[96];
            snprintf(line, sizeof(line), "%c %s  %ddBm%s",
                     i == sel_ ? '>' : ' ',
                     disp, scan_[i].rssi,
                     scan_[i].auth == WIFI_AUTH_OPEN ? "" : " [P]");

            onebit::drawBitmapText(fb, font, 10, y, line,
                                   i == sel_ ? onebit::WHITE : onebit::BLACK);
            y += font.glyph_height + 2;
        }
    } else {
        // Known tab
        if (known_count_ == 0) {
            onebit::drawBitmapText(fb, font, 10, y,
                                   "No saved networks.", onebit::BLACK);
        } else {
            // Draw the focus rect at the (possibly interpolated) y.
            onebit::fillRect(fb, 8, cur_y, fb.width() - 16,
                             font.glyph_height + 2, onebit::BLACK);
            for (int i = 0; i < known_count_ &&
                 y + font.glyph_height < fb.height() - 20; ++i) {
                char line[48];
                snprintf(line, sizeof(line), "%c %s",
                         i == sel_ ? '>' : ' ', known_[i].ssid);
                onebit::drawBitmapText(fb, font, 10, y, line,
                                       i == sel_ ? onebit::WHITE : onebit::BLACK);
                y += font.glyph_height + 2;
            }
        }
    }

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Up/Dn  Enter connect  r scan  Shift+D forget  <-/-> tab  Esc back",
        onebit::BLACK);
}

} // namespace app
