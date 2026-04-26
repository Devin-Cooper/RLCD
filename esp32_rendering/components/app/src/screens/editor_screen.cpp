#include "screens/editor_screen.hpp"
#include "overlay.hpp"
#include "screen_stack.hpp"

#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/core/framebuffer.hpp>

#include <esp_log.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef RLCD_HOST_TEST
#include "esp_heap_caps.h"
#endif

namespace app {

static const char* TAG = "editor";

EditorScreen::EditorScreen(ScreenContext& ctx, std::string path, bool new_file)
    : ctx_(ctx), path_(std::move(path)), new_file_(new_file) {
    updateBreadcrumb();
}

EditorScreen::~EditorScreen() {
    if (clipboard_) {
#ifdef RLCD_HOST_TEST
        std::free(clipboard_);
#else
        heap_caps_free(clipboard_);
#endif
        clipboard_ = nullptr;
    }
}

void EditorScreen::updateBreadcrumb() {
    bool dirty = buffer_.dirty();
    std::snprintf(breadcrumb_, sizeof(breadcrumb_), "%s%s",
                  dirty ? "* " : "", path_.c_str());
}

void EditorScreen::onEnter() {
    if (new_file_) {
        if (!buffer_.loadEmptyText()) {
            ctx_.overlay.showError("Editor", "Out of memory");
            state_ = State::BadMode;
            return;
        }
        return;
    }
    if (!buffer_.load(path_)) {
        ESP_LOGW(TAG, "load failed: %s (errno %d)", path_.c_str(), buffer_.errnoCode());
        ctx_.overlay.showError("Editor", "Failed to open file");
        state_ = State::BadMode;
        return;
    }
    if (buffer_.mode() != fb::FileBuffer::Mode::Text) {
        ctx_.overlay.showError("Editor", "Cannot edit (binary or too large)");
        state_ = State::BadMode;
        return;
    }
    if (buffer_.size() > fb::FileBuffer::kEditCapBytes) {
        ctx_.overlay.showError("Editor", "File size > 256 KB; edit not allowed");
        state_ = State::BadMode;
        return;
    }
}

void EditorScreen::handleInput(const input::InputEvent& evt, ScreenStack& stack) {
    using namespace input;
    if (state_ == State::BadMode) {
        // Any input pops back to viewer/browser.
        if (evt.source == Source::Keyboard
            && evt.type == EventType::Keypress
            && evt.data_length >= 1
            && (evt.data[0] == 0x1B || evt.data[0] == '\r' || evt.data[0] == '\n')) {
            stack.pop();
        }
        if (evt.source == Source::Button && evt.type == EventType::ButtonShort) {
            stack.pop();
        }
        return;
    }
    // Real input handling lands in Tasks 7+.
    (void)evt;
    (void)stack;
}

void EditorScreen::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    if (state_ == State::BadMode) {
        onebit::drawBitmapText(fb, font, 4, 8, "Editor unavailable.", onebit::BLACK);
        onebit::drawBitmapText(fb, font, 4, 24, "[Esc / Btn-B] back", onebit::BLACK);
        return;
    }
    renderEditor(fb, font);
}

void EditorScreen::renderEditor(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    int total = (int)buffer_.lineCount();
    viewport_rows_ = std::max(1, (fb.height() - 24) / 10);
    ensureCursorVisible();

    updateBreadcrumb();
    char head[128];
    std::snprintf(head, sizeof(head), "%s   line %d/%d col %d",
                  breadcrumb_,
                  total > 0 ? line_ + 1 : 0, total,
                  byte_col_);
    onebit::drawBitmapText(fb, font, 2, 1, head, onebit::BLACK);
    onebit::fillRect(fb, 0, 11, fb.width(), 1, onebit::BLACK);

    int y = 13;
    for (int i = 0; i < viewport_rows_; ++i) {
        int idx = scroll_top_ + i;
        if (idx >= total) break;
        renderRow(fb, font, y, idx, idx == line_);
        y += 10;
    }

    int fh = fb.height() - 12;
    onebit::fillRect(fb, 0, fh, fb.width(), 1, onebit::BLACK);
    if (find_mode_ != FindMode::Off) {
        renderFindBar(fb, font, fh + 2);
    } else {
        onebit::drawBitmapText(fb, font, 2, fh + 2,
            "^S save  ^F find  ^Z undo  Esc back",
            onebit::BLACK);
    }
}

void EditorScreen::renderFindBar(onebit::IFramebuffer&, const onebit::BitmapFont&, int) {
    // Filled in Task 12.
}

void EditorScreen::renderRow(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                              int y, int line, bool is_cursor_line) const {
    if (is_cursor_line) onebit::fillRect(fb, 0, y, fb.width(), 10, onebit::BLACK);
    auto color = is_cursor_line ? onebit::WHITE : onebit::BLACK;
    auto raw = buffer_.line((std::size_t)line);
    char buf[120];
    std::snprintf(buf, sizeof(buf), "%c %4d  %.*s",
                  is_cursor_line ? '>' : ' ',
                  line + 1,
                  (int)raw.size(), raw.data());
    onebit::drawBitmapText(fb, font, 2, y + 1, buf, color);
    // Selection-aware invert lands in Task 9.
}

SpanView<const KeybindHint> EditorScreen::keybindHints() const { return {}; }
SpanView<const Command> EditorScreen::getContextualCommands() { return {}; }
void EditorScreen::dispatchContextual(uint16_t /*id*/) {}

// Stubs filled in by later tasks:
void EditorScreen::invalidateSelection() { anchor_line_ = -1; anchor_byte_col_ = 0; }
void EditorScreen::normalizeSelection(int&, int&, int&, int&) const {}
std::size_t EditorScreen::cursorByteOffset() const { return 0; }
void EditorScreen::cursorFromByteOffset(std::size_t) {}
int EditorScreen::displayColForByteCol(int line, int byte_col) const {
    if (line < 0 || line >= (int)buffer_.lineCount()) return 0;
    auto raw_off = buffer_.lineOffset((std::size_t)line);
    auto next_off = (line + 1 < (int)buffer_.lineCount())
                    ? buffer_.lineOffset((std::size_t)line + 1)
                    : buffer_.size();
    const char* base = buffer_.data();
    int dc = 0;
    int b = 0;
    int max_b = (int)(next_off - raw_off);
    if (max_b > 0 && (raw_off + max_b - 1) < buffer_.size()
        && base[raw_off + max_b - 1] == '\n') --max_b;
    if (max_b > 0 && (raw_off + max_b - 1) < buffer_.size()
        && base[raw_off + max_b - 1] == '\r') --max_b;
    while (b < byte_col && b < max_b) {
        if (base[raw_off + b] == '\t') dc += 4 - (dc % 4);
        else ++dc;
        ++b;
    }
    return dc;
}

void EditorScreen::ensureCursorVisible() {
    if (viewport_rows_ < 1) viewport_rows_ = 1;
    if (line_ < scroll_top_) scroll_top_ = line_;
    if (line_ >= scroll_top_ + viewport_rows_) scroll_top_ = line_ - viewport_rows_ + 1;
    int dc = displayColForByteCol(line_, byte_col_);
    constexpr int kCols = 32;   // approx visible chars after gutter; tune later
    if (dc < hscroll_) hscroll_ = dc;
    if (dc >= hscroll_ + kCols) hscroll_ = dc - kCols + 1;
}
bool EditorScreen::ensureSnapshot() { return buffer_.snapshot(); }

void EditorScreen::onPrintable(char) {}
void EditorScreen::onEnterKey() {}
void EditorScreen::onBackspace() {}
void EditorScreen::onDelete() {}
void EditorScreen::onMove(int, int, bool) {}
void EditorScreen::onCopy() {}
void EditorScreen::onCut() {}
void EditorScreen::onPaste() {}
void EditorScreen::onUndo() {}
void EditorScreen::onSave(bool, ScreenStack&) {}
void EditorScreen::doSaveAtomic(const std::string&) {}
void EditorScreen::enterFindMode() {}
void EditorScreen::enterReplaceMode() {}
void EditorScreen::doReplaceCurrent() {}
void EditorScreen::onEscape(ScreenStack& stack) { stack.pop(); }

}  // namespace app
