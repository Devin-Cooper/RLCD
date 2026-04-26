#include "screens/terminal_screen.hpp"
#include "screens/dashboard_screen.hpp"
#include "screens/menu_screen.hpp"
#include "screen_stack.hpp"
#include "settings.hpp"
#include "overlay.hpp"
#include "font_for_size.hpp"
#include "ssh_client.hpp"
#include "animator.hpp"
#include "command_ids.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "term_screen";

namespace app {

namespace {
constexpr std::array<app::Command, 3> kContextual = {{
    {"Send Ctrl+C",      "",            0xFF61},
    {"Disconnect",       "",            0xFF62},
    {"Cycle font size",  "Btn B short", 0xFF63},
}};

bool matchesSeq(const input::InputEvent& e,
                const uint8_t* seq, size_t n) {
    if (e.data_length != n) return false;
    return std::memcmp(e.data, seq, n) == 0;
}

constexpr uint8_t kEsc[]         = {0x1B};
constexpr uint8_t kUp[]          = {0x1B, '[', 'A'};
constexpr uint8_t kDown[]        = {0x1B, '[', 'B'};
constexpr uint8_t kHome[]        = {0x1B, '[', 'H'};
constexpr uint8_t kEnd[]         = {0x1B, '[', 'F'};
constexpr uint8_t kPgUp[]        = {0x1B, '[', '5', '~'};
constexpr uint8_t kPgDn[]        = {0x1B, '[', '6', '~'};
constexpr uint8_t kShiftUp[]     = {0x1B, '[', '1', ';', '2', 'A'};
constexpr uint8_t kShiftDown[]   = {0x1B, '[', '1', ';', '2', 'B'};
constexpr uint8_t kShiftHome[]   = {0x1B, '[', '1', ';', '2', 'H'};
constexpr uint8_t kShiftEnd[]    = {0x1B, '[', '1', ';', '2', 'F'};
constexpr uint8_t kShiftPgUp[]   = {0x1B, '[', '5', ';', '2', '~'};
constexpr uint8_t kShiftPgDn[]   = {0x1B, '[', '6', ';', '2', '~'};
} // namespace

app::SpanView<const app::Command> TerminalScreen::getContextualCommands() {
    return app::SpanView<const app::Command>(kContextual.data(),
                                              kContextual.size());
}

void TerminalScreen::dispatchContextual(uint16_t id) {
    switch (id) {
        case 0xFF61: {
            // Forward a single 0x03 (Ctrl+C / SIGINT) byte down the SSH
            // channel — same path the keyboard handler uses.
            const uint8_t etx = 0x03;
            ctx_.sshClient.send(&etx, 1);
            break;
        }
        case 0xFF62:
            ctx_.sshClient.disconnect();
            ctx_.stack.replace(std::make_unique<DashboardScreen>(ctx_));
            break;
        case 0xFF63:
            ctx_.currentFontSize = (ctx_.currentFontSize + 1) % 3;
            ctx_.settings.font_size = ctx_.currentFontSize;
            ctx_.terminalMode.setFont(app::fontForSize(ctx_.currentFontSize));
            if (!app::saveSettings(ctx_.settings)) {
                ctx_.overlay.showError("Settings save failed",
                                        "NVS write error");
            }
            break;
        default: break;
    }
}

TerminalScreen::TerminalScreen(ScreenContext& ctx) : ctx_(ctx) {}

void TerminalScreen::onEnter() {
    ESP_LOGI(TAG, "TerminalScreen entered");
}

void TerminalScreen::feedSshData(const uint8_t* data, size_t len) {
    ctx_.terminalMode.feedData(data, len);
}

int TerminalScreen::pageLines() const {
    int rows = ctx_.terminalMode.rows();
    return rows > 1 ? rows - 1 : 1;
}

int TerminalScreen::maxOffset() const {
    return ctx_.terminalMode.scrollbackSize();
}

void TerminalScreen::enterScrollback(int initial_page_lines) {
    if (mode_ == Mode::Scrollback) return;
    int max = maxOffset();
    if (max == 0) {
        // Empty scrollback — silent no-op (the renderer's render_y_offset
        // gate prevents bouncing in Live mode).
        return;
    }
    mode_ = Mode::Scrollback;
    int new_off = std::min(initial_page_lines, max);
    scroll_offset_ = new_off;
    auto& tr = ctx_.terminalMode.renderer();
    tr.setScrollOffset(scroll_offset_);
    ctx_.terminalMode.markAllDirty();
}

void TerminalScreen::exitScrollback() {
    if (mode_ == Mode::Live) return;
    mode_ = Mode::Live;
    scroll_offset_ = 0;
    auto& tr = ctx_.terminalMode.renderer();
    tr.setScrollOffset(0);
    tr.setRenderYOffset(0);
    // Mark all rows dirty on exit. Without this, any visible row that the
    // parser hasn't touched while we were in scrollback would still hold
    // scrollback content (we bypassed clearDirty in scrollback mode).
    ctx_.terminalMode.markAllDirty();
    auto tag = makeTag(TweenKind::ScrollbackBounce,
                       bounce_id::TerminalScrollback);
    ctx_.animator.cancel(tag);
}

void TerminalScreen::scrollBy(int delta) {
    int max = maxOffset();
    int new_off = scroll_offset_ + delta;
    auto& tr = ctx_.terminalMode.renderer();

    if (new_off > max) {
        if (scroll_offset_ < max) {
            scroll_offset_ = max;
            tr.setScrollOffset(scroll_offset_);
            ctx_.terminalMode.markAllDirty();
        } else {
            bounceUp();
        }
        return;
    }
    if (new_off <= 0) {
        if (scroll_offset_ > 0) {
            scroll_offset_ = 0;
            tr.setScrollOffset(0);
            ctx_.terminalMode.markAllDirty();
        } else {
            bounceDown();
        }
        return;
    }
    scroll_offset_ = new_off;
    tr.setScrollOffset(scroll_offset_);
    ctx_.terminalMode.markAllDirty();
}

void TerminalScreen::scrollToTop() {
    int max = maxOffset();
    if (scroll_offset_ < max) {
        scroll_offset_ = max;
        ctx_.terminalMode.renderer().setScrollOffset(scroll_offset_);
        ctx_.terminalMode.markAllDirty();
    } else {
        bounceUp();
    }
}

void TerminalScreen::scrollToLive() {
    exitScrollback();
}

void TerminalScreen::bounceUp()   { triggerBounce(-4); }
void TerminalScreen::bounceDown() { triggerBounce(+4); }

void TerminalScreen::triggerBounce(int16_t from_dy) {
    auto tag = makeTag(TweenKind::ScrollbackBounce,
                       bounce_id::TerminalScrollback);
    // Animator::start with an existing same-tag slot RESTARTS in place
    // (verified in Phase 0). Rapid keymashing won't exhaust slots.
    ctx_.animator.start(tag, from_dy, /*to=*/0, kScrollbackBounceUs,
                        esp_timer_get_time());
}

void TerminalScreen::handleInput(const input::InputEvent& evt,
                                 ScreenStack& stack) {
    // Button A short → push MenuScreen
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 0) {
        stack.push(std::make_unique<MenuScreen>(ctx_));
        return;
    }

    // Button B short → cycle font size (preserves existing behavior).
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 1) {
        if (mode_ == Mode::Scrollback) exitScrollback();
        ctx_.currentFontSize = (ctx_.currentFontSize + 1) % 3;
        ctx_.settings.font_size = ctx_.currentFontSize;
        ctx_.terminalMode.setFont(app::fontForSize(ctx_.currentFontSize));
        if (!app::saveSettings(ctx_.settings)) {
            ctx_.overlay.showError("Settings save failed", "NVS write error");
        }
        return;
    }

    // Button B long → switch to Dashboard (preserves legacy semantic).
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 1) {
        stack.replace(std::make_unique<DashboardScreen>(ctx_));
        return;
    }

    // Keyboard → scrollback dispatcher (intercepts nav keys when in
    // scrollback mode), then SSH passthrough for everything else.
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {

        if (mode_ == Mode::Live) {
            // Trigger keys: Shift+PgUp or Shift+Up.
            if (matchesSeq(evt, kShiftPgUp, sizeof(kShiftPgUp)) ||
                matchesSeq(evt, kShiftUp,   sizeof(kShiftUp))) {
                enterScrollback(pageLines());
                return;
            }
            // Live mode: forward as today.
            ctx_.sshClient.send(evt.data, evt.data_length);
            return;
        }

        // mode_ == Scrollback
        // Esc must match BEFORE the catch-all so it doesn't fall through
        // to "any other keypress → exit + forward to SSH".
        if (matchesSeq(evt, kEsc, sizeof(kEsc))) {
            exitScrollback();
            return;
        }
        if (matchesSeq(evt, kShiftEnd, sizeof(kShiftEnd)) ||
            matchesSeq(evt, kEnd,      sizeof(kEnd))) {
            scrollToLive();
            return;
        }
        if (matchesSeq(evt, kShiftHome, sizeof(kShiftHome)) ||
            matchesSeq(evt, kHome,      sizeof(kHome))) {
            scrollToTop();
            return;
        }
        if (matchesSeq(evt, kShiftUp,   sizeof(kShiftUp)) ||
            matchesSeq(evt, kPgUp,      sizeof(kPgUp))) {
            scrollBy(+pageLines());
            return;
        }
        if (matchesSeq(evt, kShiftDown, sizeof(kShiftDown)) ||
            matchesSeq(evt, kPgDn,      sizeof(kPgDn))) {
            scrollBy(-pageLines());
            return;
        }
        if (matchesSeq(evt, kUp, sizeof(kUp))) {
            scrollBy(+1);
            return;
        }
        if (matchesSeq(evt, kDown, sizeof(kDown))) {
            scrollBy(-1);
            return;
        }
        // Any other keypress (including Ctrl+C 0x03) → exit scrollback,
        // then forward to SSH.
        exitScrollback();
        ctx_.sshClient.send(evt.data, evt.data_length);
        return;
    }
}

void TerminalScreen::render(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont&) {
    (void)fb;
    // TerminalMode::render() writes to its constructor-held fb ref.
    // Accepted wart — TerminalScreen is a thin pass-through.
    ctx_.terminalMode.render();
}

} // namespace app
