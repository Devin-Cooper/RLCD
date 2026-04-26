#include "screens/editor_screen.hpp"
#include "overlay.hpp"
#include "screen_stack.hpp"
#include "screens/text_input_screen.hpp"

#include <memory>

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

namespace {
// Erases the current selection in the buffer; writes the start byte-pos and
// length to out_pos / out_len. Returns false when the selection is empty.
bool eraseSelectionInBuffer(fb::FileBuffer& buf,
                            int a_l, int a_c, int b_l, int b_c,
                            std::size_t& out_pos, std::size_t& out_len) {
    if (a_l == b_l && a_c == b_c) return false;
    auto a_off = buf.lineOffset((std::size_t)a_l) + (std::size_t)a_c;
    auto b_off = buf.lineOffset((std::size_t)b_l) + (std::size_t)b_c;
    out_pos = a_off;
    out_len = b_off - a_off;
    buf.erase(a_off, b_off - a_off);
    return true;
}

char* psramAlloc(std::size_t n) {
#ifdef RLCD_HOST_TEST
    return (char*)std::malloc(n);
#else
    return (char*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
}
void psramFree(void* p) {
#ifdef RLCD_HOST_TEST
    std::free(p);
#else
    heap_caps_free(p);
#endif
}
}  // namespace

EditorScreen::EditorScreen(ScreenContext& ctx, std::string path, bool new_file)
    : ctx_(ctx), path_(std::move(path)), new_file_(new_file) {
    updateBreadcrumb();
}

EditorScreen::~EditorScreen() {
    if (clipboard_) {
        psramFree(clipboard_);
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

    // Find-mode interception (Tasks 12-13). Consumes find/replace keystrokes
    // and short-circuits the normal demuxer below.
    if (find_mode_ != FindMode::Off && len >= 1) {
        unsigned char c = data[0];
        // Esc cancels.
        if (c == 0x1B && len == 1) {
            // If user never committed in Find (find_match_idx_ < 0), restore
            // cursor to the line they came from.
            if (find_mode_ == FindMode::Find && find_match_idx_ < 0) {
                line_ = find_anchor_line_;
            }
            if (find_mode_ == FindMode::Replace) {
                // Esc from Replace → back to Find (query intact, replacement cleared).
                find_mode_ = FindMode::Find;
                replace_with_.clear();
                return;
            }
            find_mode_ = FindMode::Off;
            return;
        }
        if (find_mode_ == FindMode::Find) {
            if (c == '\r' || c == '\n' || c == 0x0E /*Ctrl-N*/) {
                if (!find_matches_.empty()) {
                    if (find_match_idx_ < 0) {
                        int target = 0;
                        for (int i = 0; i < (int)find_matches_.size(); ++i) {
                            if ((int)find_matches_[i] >= find_anchor_line_) { target = i; break; }
                        }
                        find_match_idx_ = target;
                    } else {
                        find_match_idx_ = (find_match_idx_ + 1) % (int)find_matches_.size();
                    }
                    line_ = (int)find_matches_[find_match_idx_];
                    byte_col_ = 0;
                    sticky_display_col_ = 0;
                }
                return;
            }
            if (c == 0x10 /*Ctrl-P*/) {
                if (!find_matches_.empty()) {
                    if (find_match_idx_ < 0) find_match_idx_ = (int)find_matches_.size() - 1;
                    else find_match_idx_ = (find_match_idx_ - 1 + (int)find_matches_.size())
                                           % (int)find_matches_.size();
                    line_ = (int)find_matches_[find_match_idx_];
                    byte_col_ = 0;
                    sticky_display_col_ = 0;
                }
                return;
            }
            if (c == 0x08) {  // Ctrl-H → enter Replace mode if matches exist
                if (!find_matches_.empty()) {
                    enterReplaceMode();
                } else if (!find_query_.empty()) {
                    find_query_.pop_back();
                    find_matches_ = buffer_.findAll(find_query_);
                    find_match_idx_ = -1;
                }
                return;
            }
            if (c == 0x7F) {
                if (!find_query_.empty()) {
                    find_query_.pop_back();
                    find_matches_ = buffer_.findAll(find_query_);
                    find_match_idx_ = -1;
                }
                return;
            }
            if (c >= 0x20 && c < 0x7F) {
                find_query_.push_back((char)c);
                find_matches_ = buffer_.findAll(find_query_);
                find_match_idx_ = -1;
                return;
            }
        } else /* FindMode::Replace */ {
            if (c == '\r' || c == '\n') { doReplaceCurrent(); return; }
            if (c == 0x7F) {
                if (!replace_with_.empty()) replace_with_.pop_back();
                return;
            }
            if (c == 0x08) { /* Ctrl-H in Replace mode == Backspace */
                if (!replace_with_.empty()) replace_with_.pop_back();
                return;
            }
            if (c >= 0x20 && c < 0x7F) {
                replace_with_.push_back((char)c);
                return;
            }
        }
        return;
    }

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

void EditorScreen::renderFindBar(onebit::IFramebuffer& fb, const onebit::BitmapFont& font, int y) {
    int n = (int)find_matches_.size();
    char bar[120];
    if (find_mode_ == FindMode::Find) {
        std::snprintf(bar, sizeof(bar), "Find: %s (%d/%d)   [Esc]",
                      find_query_.c_str(),
                      n == 0 ? 0 : (find_match_idx_ + 1),
                      n);
    } else {
        std::snprintf(bar, sizeof(bar), "Replace '%s' with: %s   [Enter]",
                      find_query_.c_str(),
                      replace_with_.c_str());
    }
    onebit::drawBitmapText(fb, font, 2, y, bar, onebit::BLACK);
}

void EditorScreen::renderRow(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                              int y, int line, bool is_cursor_line) const {
    // Selection range within this row (byte cols).
    bool sel = false;
    int sel_a_col = 0, sel_b_col = 0;
    if (hasSelection()) {
        int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
        if (line >= a_l && line <= b_l) {
            sel = true;
            auto raw = buffer_.line((std::size_t)line);
            sel_a_col = (line == a_l) ? a_c : 0;
            sel_b_col = (line == b_l) ? b_c : (int)raw.size();
            if (sel_a_col < 0) sel_a_col = 0;
            if (sel_b_col > (int)raw.size()) sel_b_col = (int)raw.size();
            if (sel_a_col >= sel_b_col) sel = false;
        }
    }

    // Cursor-row inversion: black band, then text drawn in WHITE.
    if (is_cursor_line) onebit::fillRect(fb, 0, y, fb.width(), 10, onebit::BLACK);
    auto base_color   = is_cursor_line ? onebit::WHITE : onebit::BLACK;
    auto select_color = is_cursor_line ? onebit::BLACK : onebit::WHITE;
    auto select_bg    = is_cursor_line ? onebit::WHITE : onebit::BLACK;

    // Row prefix (gutter): "> NNNN  " or "  NNNN  ".
    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "%c %4d  ",
                  is_cursor_line ? '>' : ' ', line + 1);
    onebit::drawBitmapText(fb, font, 2, y + 1, prefix, base_color);
    int prefix_w = onebit::getBitmapTextWidth(font, prefix);
    int text_x = 2 + prefix_w;

    auto raw = buffer_.line((std::size_t)line);
    char rowbuf[120];
    std::snprintf(rowbuf, sizeof(rowbuf), "%.*s", (int)raw.size(), raw.data());

    if (!sel) {
        onebit::drawBitmapText(fb, font, text_x, y + 1, rowbuf, base_color);
        return;
    }

    // Split the row text into 3 slices: pre / sel / post.
    // sel_*_col are byte cols within `raw`; raw was tab-expanded by line(),
    // so byte indices map 1:1 to characters (no \t in `raw`).
    char pre[120], mid[120], post[120];
    int pre_len  = std::min(sel_a_col, (int)sizeof(pre) - 1);
    int mid_len  = std::min(sel_b_col - sel_a_col, (int)sizeof(mid) - 1);
    int post_len = std::min((int)raw.size() - sel_b_col, (int)sizeof(post) - 1);
    if (pre_len < 0) pre_len = 0;
    if (mid_len < 0) mid_len = 0;
    if (post_len < 0) post_len = 0;
    std::memcpy(pre,  rowbuf, pre_len);                       pre[pre_len]   = '\0';
    std::memcpy(mid,  rowbuf + sel_a_col, mid_len);           mid[mid_len]   = '\0';
    std::memcpy(post, rowbuf + sel_b_col, post_len);          post[post_len] = '\0';

    int pre_w = onebit::getBitmapTextWidth(font, pre);
    int mid_w = onebit::getBitmapTextWidth(font, mid);

    onebit::drawBitmapText(fb, font, text_x, y + 1, pre, base_color);
    // Selection band: invert background under the mid slice, then redraw in
    // the select_color.
    onebit::fillRect(fb, text_x + pre_w, y, mid_w, 10, select_bg);
    onebit::drawBitmapText(fb, font, text_x + pre_w, y + 1, mid, select_color);
    onebit::drawBitmapText(fb, font, text_x + pre_w + mid_w, y + 1, post, base_color);
}

SpanView<const KeybindHint> EditorScreen::keybindHints() const { return {}; }
SpanView<const Command> EditorScreen::getContextualCommands() { return {}; }
void EditorScreen::dispatchContextual(uint16_t /*id*/) {}

void EditorScreen::invalidateSelection() { anchor_line_ = -1; anchor_byte_col_ = 0; }

void EditorScreen::normalizeSelection(int& a_l, int& a_c, int& b_l, int& b_c) const {
    a_l = anchor_line_; a_c = anchor_byte_col_;
    b_l = line_;        b_c = byte_col_;
    if (a_l > b_l || (a_l == b_l && a_c > b_c)) {
        std::swap(a_l, b_l);
        std::swap(a_c, b_c);
    }
}

std::size_t EditorScreen::cursorByteOffset() const {
    if (line_ >= (int)buffer_.lineCount()) return buffer_.size();
    auto off = buffer_.lineOffset((std::size_t)line_);
    return off + (std::size_t)byte_col_;
}

void EditorScreen::cursorFromByteOffset(std::size_t pos) {
    int total = (int)buffer_.lineCount();
    if (total == 0) {
        line_ = 0; byte_col_ = 0; sticky_display_col_ = 0;
        return;
    }
    for (int i = 0; i < total; ++i) {
        std::size_t a = buffer_.lineOffset((std::size_t)i);
        std::size_t b = (i + 1 < total) ? buffer_.lineOffset((std::size_t)i + 1)
                                         : buffer_.size();
        if (pos >= a && pos <= b) {
            line_ = i;
            byte_col_ = (int)(pos - a);
            sticky_display_col_ = displayColForByteCol(line_, byte_col_);
            return;
        }
    }
    // Fallback: clamp to end of last line.
    line_ = total - 1;
    byte_col_ = (int)buffer_.line((std::size_t)line_).size();
    sticky_display_col_ = displayColForByteCol(line_, byte_col_);
}

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
bool EditorScreen::ensureSnapshot() {
    if (buffer_.hasSnapshot()) return true;
    if (!buffer_.snapshot()) {
        ctx_.overlay.showError("Editor", "Undo unavailable (low PSRAM)");
    }
    return buffer_.hasSnapshot();
}

void EditorScreen::onPrintable(char c) {
    ensureSnapshot();
    if (hasSelection()) {
        int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
        std::size_t pos = 0, len = 0;
        eraseSelectionInBuffer(buffer_, a_l, a_c, b_l, b_c, pos, len);
        cursorFromByteOffset(pos);
        invalidateSelection();
    }
    char tmp[1] = { c };
    std::size_t pos = cursorByteOffset();
    if (buffer_.insert(pos, std::string_view(tmp, 1))) {
        cursorFromByteOffset(pos + 1);
    } else {
        ctx_.overlay.showError("Editor", "File size limit reached");
    }
}

void EditorScreen::onEnterKey() {
    ensureSnapshot();
    if (hasSelection()) {
        int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
        std::size_t pos = 0, len = 0;
        eraseSelectionInBuffer(buffer_, a_l, a_c, b_l, b_c, pos, len);
        cursorFromByteOffset(pos);
        invalidateSelection();
    }
    std::size_t pos = cursorByteOffset();
    std::string_view nl = buffer_.crlf() ? std::string_view("\r\n", 2)
                                          : std::string_view("\n", 1);
    if (buffer_.insert(pos, nl)) {
        cursorFromByteOffset(pos + nl.size());
    }
}

void EditorScreen::onBackspace() {
    ensureSnapshot();
    if (hasSelection()) {
        int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
        std::size_t pos = 0, len = 0;
        eraseSelectionInBuffer(buffer_, a_l, a_c, b_l, b_c, pos, len);
        cursorFromByteOffset(pos);
        invalidateSelection();
        return;
    }
    std::size_t pos = cursorByteOffset();
    if (pos == 0) return;
    std::size_t back = 1;
    if (buffer_.crlf() && pos >= 2
        && buffer_.data()[pos - 2] == '\r'
        && buffer_.data()[pos - 1] == '\n') back = 2;
    if (buffer_.erase(pos - back, back)) {
        cursorFromByteOffset(pos - back);
    }
}

void EditorScreen::onDelete() {
    ensureSnapshot();
    if (hasSelection()) {
        int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
        std::size_t pos = 0, len = 0;
        eraseSelectionInBuffer(buffer_, a_l, a_c, b_l, b_c, pos, len);
        cursorFromByteOffset(pos);
        invalidateSelection();
        return;
    }
    std::size_t pos = cursorByteOffset();
    if (pos >= buffer_.size()) return;
    std::size_t fwd = 1;
    if (buffer_.crlf() && pos + 1 < buffer_.size()
        && buffer_.data()[pos] == '\r' && buffer_.data()[pos + 1] == '\n') fwd = 2;
    buffer_.erase(pos, fwd);
}
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
void EditorScreen::onCopy() {
    if (!hasSelection()) return;
    int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
    std::size_t a = buffer_.lineOffset((std::size_t)a_l) + (std::size_t)a_c;
    std::size_t b = buffer_.lineOffset((std::size_t)b_l) + (std::size_t)b_c;
    std::size_t len = b - a;
    if (len > kClipCapBytes) {
        ctx_.overlay.showError("Editor", "Selection too large (16 KB cap)");
        return;
    }
    if (clipboard_) psramFree(clipboard_);
    clipboard_ = psramAlloc(len > 0 ? len : 1);
    if (!clipboard_) { clipboard_size_ = 0; return; }
    if (len > 0) std::memcpy(clipboard_, buffer_.data() + a, len);
    clipboard_size_ = len;
}

void EditorScreen::onCut() {
    if (!hasSelection()) return;
    onCopy();
    ensureSnapshot();
    int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
    std::size_t pos = 0, len = 0;
    eraseSelectionInBuffer(buffer_, a_l, a_c, b_l, b_c, pos, len);
    cursorFromByteOffset(pos);
    invalidateSelection();
}

void EditorScreen::onPaste() {
    if (!clipboard_ || clipboard_size_ == 0) return;
    if (buffer_.size() + clipboard_size_ > fb::FileBuffer::kEditCapBytes) {
        ctx_.overlay.showError("Editor", "Paste exceeds 256 KB cap");
        return;
    }
    ensureSnapshot();
    if (hasSelection()) {
        int a_l, a_c, b_l, b_c; normalizeSelection(a_l, a_c, b_l, b_c);
        std::size_t pos = 0, len = 0;
        eraseSelectionInBuffer(buffer_, a_l, a_c, b_l, b_c, pos, len);
        cursorFromByteOffset(pos);
        invalidateSelection();
    }
    std::size_t pos = cursorByteOffset();
    if (buffer_.insert(pos, std::string_view(clipboard_, clipboard_size_))) {
        cursorFromByteOffset(pos + clipboard_size_);
    }
}
void EditorScreen::onUndo() {
    if (!buffer_.hasSnapshot()) return;
    if (buffer_.swapWithSnapshot()) {
        // Cursor may now point past EOF; clamp.
        std::size_t pos = std::min(cursorByteOffset(), buffer_.size());
        cursorFromByteOffset(pos);
        invalidateSelection();
    }
}
void EditorScreen::onSave(bool save_as, ScreenStack& /*stack*/) {
    if (save_as || new_file_) {
        ctx_.stack.push(std::make_unique<TextInputScreen>(ctx_,
            "Save as", path_.c_str(),
            [this](TextInputResult r, const std::string& v) {
                if (r != TextInputResult::Submit || v.empty()) return;
                doSaveAtomic(v);
            }));
        return;
    }
    doSaveAtomic(path_);
}

void EditorScreen::doSaveAtomic(const std::string& dest) {
    // Show a "Saving..." toast for the duration of the synchronous save.
    // OverlayManager::showInfo is non-blocking; the save below blocks for
    // ~80 ms on FATFS. The toast will appear on the next frame.
    ctx_.overlay.showInfo("Saving", dest.c_str());
    if (buffer_.saveAtomic(dest)) {
        buffer_.clearDirty();
        path_ = dest;
        new_file_ = false;
        updateBreadcrumb();
    } else {
        char body[80];
        std::snprintf(body, sizeof(body), "Save failed: %s",
                      std::strerror(buffer_.errnoCode()));
        ctx_.overlay.showError("Editor", body);
    }
}
void EditorScreen::enterFindMode() {
    find_mode_ = FindMode::Find;
    find_query_.clear();
    find_matches_.clear();
    find_match_idx_ = -1;
    find_anchor_line_ = line_;
}
void EditorScreen::enterReplaceMode() {
    if (find_mode_ != FindMode::Find || find_matches_.empty()) return;
    find_mode_ = FindMode::Replace;
    replace_with_.clear();
}

void EditorScreen::doReplaceCurrent() {
    if (find_mode_ != FindMode::Replace || find_matches_.empty()) return;
    if (find_match_idx_ < 0) {
        // Pick first-at-or-after anchor.
        int target = 0;
        for (int i = 0; i < (int)find_matches_.size(); ++i) {
            if ((int)find_matches_[i] >= find_anchor_line_) { target = i; break; }
        }
        find_match_idx_ = target;
    }
    ensureSnapshot();
    int match_line = (int)find_matches_[find_match_idx_];
    auto raw = buffer_.line((std::size_t)match_line);
    auto needle = find_query_;
    auto to_lower = [](char c) {
        return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    };
    int found_col = -1;
    for (std::size_t i = 0; i + needle.size() <= raw.size(); ++i) {
        bool eq = true;
        for (std::size_t k = 0; k < needle.size(); ++k) {
            if (to_lower(raw[i + k]) != to_lower(needle[k])) { eq = false; break; }
        }
        if (eq) { found_col = (int)i; break; }
    }
    if (found_col < 0) return;
    std::size_t a = buffer_.lineOffset((std::size_t)match_line) + (std::size_t)found_col;
    buffer_.erase(a, needle.size());
    buffer_.insert(a, replace_with_);
    // Recompute matches (positions may have shifted).
    find_matches_ = buffer_.findAll(find_query_);
    if (find_matches_.empty()) {
        find_mode_ = FindMode::Find;
        find_match_idx_ = -1;
        return;
    }
    // Advance to next match.
    find_match_idx_ = std::min(find_match_idx_, (int)find_matches_.size() - 1);
    line_ = (int)find_matches_[find_match_idx_];
    byte_col_ = 0;
}
void EditorScreen::onEscape(ScreenStack& stack) {
    if (find_mode_ != FindMode::Off) {
        // Esc is handled inside the find/replace handler (Task 12).
        return;
    }
    if (hasSelection()) { invalidateSelection(); return; }
    if (!buffer_.dirty()) { stack.pop(); return; }

    char body[140];
    std::snprintf(body, sizeof(body), "%s\nSave before closing?", path_.c_str());
    ScreenStack* stack_ptr = &stack;
    ctx_.overlay.showThreeWay("Unsaved changes", body,
        std::array<const char*, 3>{ "Save", "Discard", "Cancel" },
        [this, stack_ptr](int choice) {
            if (choice == 0) {
                doSaveAtomic(path_);
                if (!buffer_.dirty()) stack_ptr->pop();
            } else if (choice == 1) {
                if (buffer_.hasSnapshot()) buffer_.restoreFromSnapshot();
                buffer_.clearDirty();
                stack_ptr->pop();
            }
            // choice == 2: cancel — stay
        });
}

}  // namespace app
