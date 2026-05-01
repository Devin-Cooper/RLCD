#include "screens/dashboard_screen.hpp"
#include "screens/menu_screen.hpp"
#include "screen_stack.hpp"
#include "dashboard_card_set.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstdio>
#include <cstring>

static const char* TAG = "dash_screen";

namespace app {

DashboardScreen::DashboardScreen(ScreenContext& ctx) : ctx_(ctx) {
    rebuildCards();
}

void DashboardScreen::onEnter() {
    ESP_LOGI(TAG, "DashboardScreen entered (%d cards)", card_count_);
    rebuildCards();
    int64_t now_ms = esp_timer_get_time() / 1000;
    card_started_ms_ = now_ms;
}

void DashboardScreen::rebuildCards() {
    card_count_ = buildDashboardCardSet(ctx_.dashboard, cards_.data(),
                                        kCardArraySize);
    if (current_card_ >= card_count_) {
        current_card_ = (card_count_ > 0) ? card_count_ - 1 : 0;
    }
    prev_card_ = current_card_;
}

void DashboardScreen::tickUpdate(int64_t now_ms) {
    ctx_.dashboard.update(ctx_.sshClient, now_ms);

    int n = ctx_.dashboard.commandCount() + 1;
    if (n != card_count_) rebuildCards();

    if (card_count_ <= 1) return;

    uint16_t dwell_ms = ctx_.settings.dashboard_card_dwell_ms;
    if (dwell_ms < 1000) dwell_ms = 1000;
    if (now_ms - card_started_ms_ >= dwell_ms) {
        advance(+1, now_ms);
    }
}

void DashboardScreen::advance(int delta, int64_t now_ms) {
    if (card_count_ <= 0) return;
    prev_card_ = current_card_;
    current_card_ = wrapCardIndex(current_card_, delta, card_count_);
    card_started_ms_ = now_ms;

    int16_t from_x = pipUnderlineXFor(prev_card_, card_count_);
    int16_t to_x   = pipUnderlineXFor(current_card_, card_count_);
    ctx_.animator.start(kPipTag, from_x, to_x, kDashboardPipUs, now_ms * 1000);
}

void DashboardScreen::jumpTo(int idx, int64_t now_ms) {
    if (idx < 0 || idx >= card_count_ || idx == current_card_) return;
    prev_card_ = current_card_;
    current_card_ = idx;
    card_started_ms_ = now_ms;

    int16_t from_x = pipUnderlineXFor(prev_card_, card_count_);
    int16_t to_x   = pipUnderlineXFor(current_card_, card_count_);
    ctx_.animator.start(kPipTag, from_x, to_x, kDashboardPipUs, now_ms * 1000);
}

void DashboardScreen::feedSshData(const uint8_t* data, size_t len) {
    ctx_.dashboard.feedData(data, len);
}

void DashboardScreen::handleInput(const input::InputEvent& evt,
                                  ScreenStack& stack) {
    int64_t now_ms = esp_timer_get_time() / 1000;

    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 0) {
        stack.push(std::make_unique<MenuScreen>(ctx_));
        return;
    }
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 1) {
        advance(+1, now_ms);
        return;
    }
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 0) {
        advance(-1, now_ms);
        return;
    }
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 1) {
        if (ctx_.switchToNextServer) ctx_.switchToNextServer();
        return;
    }

    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) return;

    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        if (evt.data[2] == 'C') { advance(+1, now_ms); return; }
        if (evt.data[2] == 'D') { advance(-1, now_ms); return; }
    }

    if (evt.data_length == 1) {
        char c = evt.data[0];
        if (c == 0x0E) { advance(+1, now_ms); return; }
        if (c == 0x10) { advance(-1, now_ms); return; }
        if (c >= '1' && c <= '9') {
            int idx = c - '1';
            if (idx < card_count_) jumpTo(idx, now_ms);
            return;
        }
    }
}

void DashboardScreen::render(onebit::IFramebuffer& fb,
                             const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    if (card_count_ == 0) {
        renderEmptyState(fb, font);
        return;
    }
    const DashboardSnapshot& snap = ctx_.dashboard.snapshot();
    renderCard(fb, font, cards_[current_card_], snap);
    if (card_count_ > 1) {
        int64_t now_us = esp_timer_get_time();
        int16_t underline_x = ctx_.animator.value(kPipTag, now_us);
        if (underline_x == 0) {
            underline_x = pipUnderlineXFor(current_card_, card_count_);
        }
        renderPipStrip(fb, current_card_, prev_card_, card_count_, underline_x);
    }
}

} // namespace app
