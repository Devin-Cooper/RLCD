#include "screens/file_viewer_screen.hpp"
#include "screen_stack.hpp"
#include "path_helpers.hpp"
#include "overlay.hpp"

#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/core/framebuffer.hpp>

#include <esp_log.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace app {

static const char* TAG = "file_viewer";

FileViewerScreen::FileViewerScreen(ScreenContext& ctx, std::string path)
    : ctx_(ctx), path_(std::move(path)) {
    auto bn = fb::path::basename(path_);
    std::snprintf(breadcrumb_, sizeof(breadcrumb_), "%.*s", (int)bn.size(), bn.data());
}

void FileViewerScreen::onEnter() {
    if (!buffer_.load(path_)) {
        ESP_LOGW(TAG, "load failed: %s (errno %d)", path_.c_str(), buffer_.errnoCode());
    }
}

void FileViewerScreen::handleInput(const input::InputEvent& evt, ScreenStack& stack) {
    using namespace input;
    if (evt.source == Source::Button && evt.type == EventType::ButtonShort && evt.button_id == 1) {
        stack.pop(); return;
    }
    if (evt.source == Source::Keyboard && evt.type == EventType::Keypress
        && evt.data_length == 1 && evt.data[0] == 0x1B) {
        if (search_active_) { search_active_ = false; return; }
        stack.pop(); return;
    }

    if (buffer_.mode() == fb::FileBuffer::Mode::Text
        && evt.source == Source::Keyboard && evt.type == EventType::Keypress) {
        int total = (int)buffer_.lineCount();
        int rows = std::max(1, (ctx_.fb.height() - 24) / 10);
        if (evt.data_length >= 6 && evt.data[0] == 0x1B && evt.data[1] == 0x5B
            && evt.data[2] == 0x31 && evt.data[3] == 0x3B && evt.data[4] == 0x32) {
            if (evt.data[5] == 'A') cursor_ = std::max(0, cursor_ - rows);
            else if (evt.data[5] == 'B') cursor_ = std::min(total - 1, cursor_ + rows);
            return;
        }
        if (evt.data_length >= 3 && evt.data[0] == 0x1B && evt.data[1] == 0x5B) {
            if (evt.data[2] == 'A') { if (cursor_ > 0) --cursor_; return; }
            if (evt.data[2] == 'B') { if (cursor_ < total - 1) ++cursor_; return; }
        }
        if (evt.data_length == 1) {
            if (evt.data[0] == '\r' || evt.data[0] == '\n') {
                // Enter: in search-active mode, jump to next match (Task 18). Here noop.
                return;
            }
        }
    }
}

void FileViewerScreen::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    switch (buffer_.mode()) {
        case fb::FileBuffer::Mode::Text:     renderText(fb, font); break;
        case fb::FileBuffer::Mode::Hex:      renderHex(fb, font); break;
        case fb::FileBuffer::Mode::TooLarge: renderTooLarge(fb, font); break;
        case fb::FileBuffer::Mode::Error:    renderError(fb, font); break;
    }
}

void FileViewerScreen::renderText(onebit::IFramebuffer& fb,
                                  const onebit::BitmapFont& font) {
    int rows = std::max(1, (fb.height() - 24) / 10);
    int total = (int)buffer_.lineCount();

    if (cursor_ < scroll_top_) scroll_top_ = cursor_;
    if (cursor_ >= scroll_top_ + rows) scroll_top_ = cursor_ - rows + 1;

    char head[80];
    std::snprintf(head, sizeof(head), "%s   line %d/%d",
                  path_.c_str(), cursor_ + 1, total);
    onebit::drawBitmapText(fb, font, 2, 1, head, onebit::BLACK);
    onebit::fillRect(fb, 0, 11, fb.width(), 1, onebit::BLACK);

    int y = 13;
    for (int i = 0; i < rows; ++i) {
        int idx = scroll_top_ + i;
        if (idx >= total) break;
        bool sel = (idx == cursor_);
        if (sel) onebit::fillRect(fb, 0, y, fb.width(), 10, onebit::BLACK);
        char buf[256];
        auto line = buffer_.line((std::size_t)idx);
        std::snprintf(buf, sizeof(buf), "%c %4d  %.*s",
                      sel ? '>' : ' ',
                      idx + 1,
                      (int)line.size(), line.data());
        onebit::drawBitmapText(fb, font, 2, y + 1, buf,
                               sel ? onebit::WHITE : onebit::BLACK);
        y += 10;
    }

    int fh = fb.height() - 12;
    onebit::fillRect(fb, 0, fh, fb.width(), 1, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 2, fh + 2,
                           "up/dn line  Sh+up/dn page  Ctrl-F find  Esc back",
                           onebit::BLACK);
}
void FileViewerScreen::renderHex(onebit::IFramebuffer&, const onebit::BitmapFont&) {}
void FileViewerScreen::renderTooLarge(onebit::IFramebuffer&, const onebit::BitmapFont&) {}

void FileViewerScreen::renderError(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    char title[80];
    std::snprintf(title, sizeof(title), "%s", path_.c_str());
    onebit::drawBitmapText(fb, font, 4, 4, title, onebit::BLACK);

    char msg[80];
    std::snprintf(msg, sizeof(msg), "Failed to open: %s (errno %d)",
                  std::strerror(buffer_.errnoCode()), buffer_.errnoCode());
    onebit::drawBitmapText(fb, font, 4, 28, msg, onebit::BLACK);

    int fh = fb.height() - 12;
    onebit::drawBitmapText(fb, font, 4, fh, "[Back]  Esc", onebit::BLACK);
}

SpanView<const KeybindHint> FileViewerScreen::keybindHints() const { return {}; }
SpanView<const Command> FileViewerScreen::getContextualCommands() { return {}; }
void FileViewerScreen::dispatchContextual(uint16_t /*id*/) {}

}  // namespace app
