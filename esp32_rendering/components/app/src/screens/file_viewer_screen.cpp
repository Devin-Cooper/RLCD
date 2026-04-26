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
    // Per-mode handlers added by Tasks 16/17/18.
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

void FileViewerScreen::renderText(onebit::IFramebuffer&, const onebit::BitmapFont&) {}
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
