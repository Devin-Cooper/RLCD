#include "screens/timezone_screen.hpp"
#include "screens/text_input_screen.hpp"
#include "time_service.hpp"
#include "screen_stack.hpp"
#include "text_input.hpp"   // for TextInputResult
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

namespace app {

TimezoneScreen::TimezoneScreen(ScreenContext& ctx) : ctx_(ctx) {
    size_t count = 0;
    auto* cat = time_service::TimeService::catalog(&count);
    for (size_t i = 0; i < count; ++i) {
        if (std::strcmp(cat[i].posix_tz, ctx_.timeService.timezone()) == 0) {
            selected_ = static_cast<int>(i);
            break;
        }
    }
}

void TimezoneScreen::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    onebit::drawBitmapText(fb, font, 10, 10, "Time zone", onebit::BLACK);

    size_t count = 0;
    auto* cat = time_service::TimeService::catalog(&count);
    int total = static_cast<int>(count) + 1;  // +1 for "Other"

    // Keep `selected_` visible: scroll the window so it sits within
    // [scrollTop_, scrollTop_ + visible_rows).
    const int line_h = 18;
    const int top_y = 40;
    const int max_y = static_cast<int>(fb.height()) - line_h;
    int visible_rows = (max_y - top_y) / line_h;
    if (visible_rows < 1) visible_rows = 1;
    if (selected_ < scrollTop_) scrollTop_ = selected_;
    if (selected_ >= scrollTop_ + visible_rows)
        scrollTop_ = selected_ - visible_rows + 1;
    if (scrollTop_ < 0) scrollTop_ = 0;

    int y = top_y;
    for (int i = scrollTop_; i < total && y < max_y; ++i, y += line_h) {
        const char* label = (i < (int)count) ? cat[i].label : "Other (POSIX TZ)...";
        const char* prefix = (i == selected_) ? ">" : " ";
        char line[64];
        std::snprintf(line, sizeof(line), "%s %s", prefix, label);
        onebit::drawBitmapText(fb, font, 14, y, line, onebit::BLACK);
    }
}

void TimezoneScreen::handleInput(const input::InputEvent& evt,
                                 ScreenStack& stack) {
    using ET = input::EventType;
    size_t count = 0;
    auto* cat = time_service::TimeService::catalog(&count);
    int total = static_cast<int>(count) + 1;

    auto applySelection = [&]() {
        if (selected_ < (int)count) {
            ctx_.timeService.setTimezone(cat[selected_].posix_tz);
            stack.pop();
        } else {
            // TextInputScreen takes (ctx, title, initial, on_done, opts).
            // Callback is `void(TextInputResult, const std::string&)`. The
            // screen pops itself before invoking the callback, so we MUST
            // pop ourselves separately on success.
            const char* current = ctx_.timeService.timezone();
            stack.push(std::make_unique<TextInputScreen>(
                ctx_, "POSIX TZ", current,
                [this](TextInputResult r, const std::string& s) {
                    if (r == TextInputResult::Submit && !s.empty()) {
                        ctx_.timeService.setTimezone(s.c_str());
                        ctx_.stack.pop();   // pop self (TimezoneScreen)
                    }
                }));
        }
    };

    // Buttons
    if (evt.source == input::Source::Button && evt.type == ET::ButtonShort) {
        selected_ = (selected_ + 1) % total;
        return;
    }
    if (evt.source == input::Source::Button && evt.type == ET::ButtonLong) {
        applySelection();
        return;
    }
    // Keyboard
    if (evt.source != input::Source::Keyboard || evt.type != ET::Keypress) return;
    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        if (evt.data[2] == 'A') selected_ = (selected_ + total - 1) % total;
        if (evt.data[2] == 'B') selected_ = (selected_ + 1) % total;
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == '\r') { applySelection(); return; }
    if (evt.data_length == 1 && evt.data[0] == 0x1B) { stack.pop(); return; }
}

}  // namespace app
