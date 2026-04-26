#include "screens/command_palette.hpp"
#include "screen_stack.hpp"
#include "command_dispatcher.hpp"
#include "command_palette_filter.hpp"
#include "animator.hpp"
#include "command_ids.hpp"
#include <esp_timer.h>
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <cstdio>

namespace app {

namespace {
constexpr int kMaxVisibleRows = 8;
constexpr int16_t kBoxW = 340;
constexpr int16_t kBoxH = 220;
} // namespace

CommandPalette::CommandPalette(ScreenContext& ctx)
    : ctx_(ctx),
      query_(query_buf_, sizeof(query_buf_), TextInputOpts{}) {}

void CommandPalette::onEnter() {
    query_.clear();
    CommandRegistry::instance().refreshDynamicServerCommands(ctx_.configMgr);
    rebuildFiltered();
}

void CommandPalette::rebuildFiltered() {
    filtered_.clear();
    // Contextual first: query the screen UNDER the palette.
    if (ctx_.stack.depth() >= 2) {
        Screen* under = ctx_.stack.at(ctx_.stack.depth() - 2);
        if (under) {
            for (const auto& c : under->getContextualCommands()) {
                if (icontains(c.title, query_buf_)) filtered_.push_back(c);
            }
        }
    }
    auto& reg = CommandRegistry::instance();
    for (const auto& c : reg.globals()) {
        if (icontains(c.title, query_buf_)) filtered_.push_back(c);
    }
    for (const auto& c : reg.dynamicServerCommands()) {
        if (icontains(c.title, query_buf_)) filtered_.push_back(c);
    }
    if (selected_ >= static_cast<int>(filtered_.size())) {
        selected_ = filtered_.empty() ? 0 : static_cast<int>(filtered_.size()) - 1;
        // Filter shrunk — cancel any in-flight focus tween, snap to new index.
        auto tag = makeTag(TweenKind::FocusRect, focus_id::CommandPalette);
        ctx_.animator.cancel(tag);
        if (focus_y_initialized_) prev_selected_y_ = computeRowY(selected_);
    }
}

int16_t CommandPalette::computeRowY(int idx) const {
    return list_start_y_ + static_cast<int16_t>(idx * row_h_);
}

void CommandPalette::handleInput(const input::InputEvent& evt, ScreenStack& stack) {
    // Esc / Btn A short → pop
    if ((evt.source == input::Source::Keyboard
         && evt.type == input::EventType::Keypress
         && evt.data_length >= 1 && evt.data[0] == 0x1B
         && evt.data_length == 1)
        || (evt.source == input::Source::Button
            && evt.type == input::EventType::ButtonShort
            && evt.button_id == 0)) {
        stack.pop();
        return;
    }

    // Enter → dispatch + (maybe) self-pop. Intercepted BEFORE TextInput so
    // the line never gets "submitted" into the query buffer.
    if (evt.source == input::Source::Keyboard
        && evt.type == input::EventType::Keypress
        && evt.data_length == 1
        && (evt.data[0] == '\r' || evt.data[0] == '\n')) {
        if (selected_ >= 0 && selected_ < static_cast<int>(filtered_.size())) {
            uint16_t id = filtered_[selected_].id;
            DispatchResult r = dispatchCommand(id, ctx_);
            if (r == DispatchResult::ScreenStays) {
                stack.pop();   // close palette; dispatcher didn't change stack
            }
            // else: dispatcher already did stack.replace(), palette is gone
        }
        return;
    }

    // Btn B short → move down
    if (evt.source == input::Source::Button
        && evt.type == input::EventType::ButtonShort
        && evt.button_id == 1) {
        int next = selected_ + 1;
        if (next < static_cast<int>(filtered_.size())) {
            int16_t old_y = computeRowY(selected_);
            selected_ = next;
            int16_t new_y = computeRowY(selected_);
            if (focus_y_initialized_) {
                auto tag = makeTag(TweenKind::FocusRect, focus_id::CommandPalette);
                ctx_.animator.start(tag, old_y, new_y, kFocusRectUs, esp_timer_get_time());
                prev_selected_y_ = new_y;
            }
        }
        return;
    }

    // Arrow keys (HID arrow → ESC [ A/B)
    if (evt.source == input::Source::Keyboard
        && evt.type == input::EventType::Keypress
        && evt.data_length == 3
        && evt.data[0] == 0x1B && evt.data[1] == '['
        && (evt.data[2] == 'A' || evt.data[2] == 'B')) {
        int next = selected_ + (evt.data[2] == 'B' ? +1 : -1);
        if (next < 0) next = 0;
        if (next >= static_cast<int>(filtered_.size())) {
            next = filtered_.empty() ? 0 : static_cast<int>(filtered_.size()) - 1;
        }
        if (next != selected_) {
            int16_t old_y = computeRowY(selected_);
            selected_ = next;
            int16_t new_y = computeRowY(selected_);
            if (focus_y_initialized_) {
                auto tag = makeTag(TweenKind::FocusRect, focus_id::CommandPalette);
                ctx_.animator.start(tag, old_y, new_y, kFocusRectUs, esp_timer_get_time());
                prev_selected_y_ = new_y;
            }
        }
        return;
    }

    // Everything else → forward to TextInput, then re-filter.
    if (evt.source == input::Source::Keyboard
        && evt.type == input::EventType::Keypress) {
        query_.handleKey(evt.data, evt.data_length);
        rebuildFiltered();
    }
}

void CommandPalette::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    // Centered scaled box (use static for now; scale-tween could be added)
    int16_t fb_w = fb.width();
    int16_t fb_h = fb.height();
    int16_t x = (fb_w - kBoxW) / 2;
    int16_t y = (fb_h - kBoxH) / 2;
    onebit::fillRect(fb, x, y, kBoxW, kBoxH, onebit::WHITE);
    onebit::drawRect(fb, x, y, kBoxW, kBoxH, onebit::BLACK);

    // Search row
    int16_t row_y = y + 6;
    char prompt[40];
    std::snprintf(prompt, sizeof(prompt), "> %s", query_buf_);
    onebit::drawBitmapText(fb, font, x + 6, row_y, prompt, onebit::BLACK);
    row_y += font.glyph_height + 4;

    // Hairline separator
    onebit::fillRect(fb, x + 4, row_y, kBoxW - 8, 1, onebit::BLACK);
    row_y += 4;

    // List
    list_start_y_ = row_y;
    row_h_ = static_cast<int16_t>(font.glyph_height + 2);

    if (!focus_y_initialized_ && !filtered_.empty()) {
        prev_selected_y_ = computeRowY(selected_);
        focus_y_initialized_ = true;
    }

    if (filtered_.empty()) {
        onebit::drawBitmapText(fb, font, x + 12, row_y, "(no matches)", onebit::BLACK);
    } else {
        // Focus rect
        auto tag = makeTag(TweenKind::FocusRect, focus_id::CommandPalette);
        int64_t now = esp_timer_get_time();
        int16_t cur_y = ctx_.animator.inProgress(tag, now)
                      ? ctx_.animator.value(tag, now)
                      : prev_selected_y_;
        onebit::fillRect(fb, x + 4, cur_y, kBoxW - 8, row_h_, onebit::BLACK);

        // Visible rows
        int max_rows = kMaxVisibleRows;
        int total = static_cast<int>(filtered_.size());
        int first = 0;
        if (total > max_rows) {
            // Center selected within visible window
            first = selected_ - max_rows / 2;
            if (first < 0) first = 0;
            if (first + max_rows > total) first = total - max_rows;
        }
        for (int i = first; i < first + max_rows && i < total; ++i) {
            int16_t y_row = computeRowY(i);
            const auto& c = filtered_[i];
            auto color = (i == selected_) ? onebit::WHITE : onebit::BLACK;
            onebit::drawBitmapText(fb, font, x + 8, y_row, c.title, color);
            // hint right-aligned
            if (c.hint[0]) {
                int16_t hw = onebit::getBitmapTextWidth(font, c.hint);
                onebit::drawBitmapText(fb, font, x + kBoxW - 6 - hw, y_row,
                                       c.hint, color);
            }
        }
    }

    // Footer (intra-box) — keybind footer is suppressed via wantsKeybindFooter().
    int16_t footer_y = y + kBoxH - font.glyph_height - 4;
    onebit::drawBitmapText(fb, font, x + 6, footer_y,
                           "[up/dn] pick  Enter run  Esc close",
                           onebit::BLACK);
}

} // namespace app
