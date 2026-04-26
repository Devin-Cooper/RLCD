#include "screens/editor_screen.hpp"
#include "overlay.hpp"
#include "screen_stack.hpp"

#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/core/framebuffer.hpp>

#include <esp_log.h>
#include <algorithm>
#include <climits>
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

    if (evt.source == Source::Button) {
        if (evt.type == EventType::ButtonShort && evt.button_id == 0) onEnterKey();
        else if (evt.type == EventType::ButtonShort && evt.button_id == 1) onEscape(stack);
        return;
    }
    if (evt.source != Source::Keyboard || evt.type != EventType::Keypress) return;

    auto data = evt.data;
    auto len  = evt.data_length;

    // Find-mode interception lands in Task 12; pass through for now.

    // Shift+arrow = ESC [ 1 ; 2 [A/B/C/D/H/F]
    if (len >= 6 && data[0] == 0x1B && data[1] == 0x5B && data[2] == 0x31
        && data[3] == 0x3B && data[4] == 0x32) {
        switch (data[5]) {
            case 'A': onMove(-1, 0, true); return;
            case 'B': onMove( 1, 0, true); return;
            case 'C': onMove( 0, 1, true); return;
            case 'D': onMove( 0,-1, true); return;
            case 'H': onMove(-line_, -byte_col_, true); return;   // Shift+Home
            case 'F': onMove(0, INT_MAX, true);              return;   // Shift+End
        }
    }
    // Ctrl+Home/End = ESC [ 1 ; 5 H / F
    if (len >= 6 && data[0] == 0x1B && data[1] == 0x5B && data[2] == 0x31
        && data[3] == 0x3B && data[4] == 0x35) {
        if (data[5] == 'H') {
            line_ = 0; byte_col_ = 0; sticky_display_col_ = 0;
            invalidateSelection();
            return;
        }
        if (data[5] == 'F') {
            line_ = (int)buffer_.lineCount() - 1;
            if (line_ < 0) line_ = 0;
            byte_col_ = (int)buffer_.line((std::size_t)line_).size();
            sticky_display_col_ = displayColForByteCol(line_, byte_col_);
            invalidateSelection();
            return;
        }
    }
    // Plain CSI: arrows + Home/End + Delete
    if (len >= 3 && data[0] == 0x1B && data[1] == 0x5B) {
        switch (data[2]) {
            case 'A': onMove(-1, 0, false); return;
            case 'B': onMove( 1, 0, false); return;
            case 'C': onMove( 0, 1, false); return;
            case 'D': onMove( 0,-1, false); return;
            case 'H': onMove(0, -byte_col_, false); return;        // Home
            case 'F': onMove(0,  INT_MAX,    false); return;        // End
            case '3':
                if (len >= 4 && data[3] == '~') { onDelete(); return; }
                break;
        }
    }
    // Single byte
    if (len == 1) {
        unsigned char c = data[0];
        if (c == 0x1B) { onEscape(stack); return; }
        if (c == '\r' || c == '\n') { onEnterKey(); return; }
        if (c == 0x09) { onPrintable('\t'); return; }
        if (c == 0x7F) { onBackspace(); return; }
        if (c == 0x08) {
            // Ctrl-H ambiguous: in find-mode-with-matches it enters Replace;
            // otherwise treat as Backspace.
            if (find_mode_ == FindMode::Find && !find_matches_.empty()) {
                enterReplaceMode();
            } else {
                onBackspace();
            }
            return;
        }
        if (c == 0x01) {                         // Ctrl-A select all
            anchor_line_ = 0; anchor_byte_col_ = 0;
            line_ = (int)buffer_.lineCount() - 1;
            if (line_ < 0) line_ = 0;
            byte_col_ = (int)buffer_.line((std::size_t)line_).size();
            return;
        }
        if (c == 0x03) { onCopy(); return; }     // Ctrl-C
        if (c == 0x18) { onCut();  return; }     // Ctrl-X
        if (c == 0x16) { onPaste(); return; }    // Ctrl-V
        if (c == 0x1A) { onUndo();  return; }    // Ctrl-Z
        if (c == 0x06) { enterFindMode(); return; }  // Ctrl-F
        if (c == 0x13) { onSave(/*save_as=*/false, stack); return; }  // Ctrl-S
        if (c == 0x0B) {
            // Ctrl-K palette — handled by global interceptor; no-op here.
            return;
        }
        if (c >= 0x20 && c < 0x7F) { onPrintable((char)c); return; }
    }
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
void EditorScreen::onMove(int dline, int dcol, bool extend) {
    if (!extend) invalidateSelection();
    else if (anchor_line_ < 0) { anchor_line_ = line_; anchor_byte_col_ = byte_col_; }

    int total = (int)buffer_.lineCount();
    if (total == 0) return;

    if (dline != 0) {
        // Vertical move uses sticky_display_col_ for the target column.
        int target_dc = sticky_display_col_;
        int new_line = std::clamp(line_ + dline, 0, total - 1);
        line_ = new_line;
        // Find byte_col_ that gives display_col closest to target_dc.
        auto raw = buffer_.line((std::size_t)line_);
        int dc = 0; int b = 0;
        for (; b < (int)raw.size() && dc < target_dc; ++b) {
            if (raw[b] == '\t') dc += 4 - (dc % 4);
            else ++dc;
        }
        byte_col_ = b;
    }
    if (dcol != 0) {
        auto raw = buffer_.line((std::size_t)line_);
        byte_col_ = std::clamp(byte_col_ + dcol, 0, (int)raw.size());
        sticky_display_col_ = displayColForByteCol(line_, byte_col_);
    }
}
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
