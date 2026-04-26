#include "screens/file_viewer_screen.hpp"
#include "screen_stack.hpp"
#include "path_helpers.hpp"
#include "overlay.hpp"
#include "command_registry.hpp"
#include "screens/text_input_screen.hpp"
#include "screens/editor_screen.hpp"
#include <cstdlib>
#include <memory>

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

    // Search-active interception (Text mode only). Must run BEFORE the generic
    // Esc-pop arm so Esc cancels the search rather than popping the screen.
    if (search_active_ && buffer_.mode() == fb::FileBuffer::Mode::Text
        && evt.source == Source::Keyboard && evt.type == EventType::Keypress
        && evt.data_length >= 1) {
        unsigned char c = evt.data[0];
        if (c == 0x1B && evt.data_length == 1) {
            // Esc: cancel; return cursor to anchor if no commit happened.
            search_active_ = false;
            if (search_match_idx_ < 0) cursor_ = search_anchor_line_;
            return;
        }
        if (c == '\r' || c == '\n' || c == 0x0E /*Ctrl-N*/) {
            if (!search_matches_.empty()) {
                if (search_match_idx_ < 0) {
                    // First commit: jump to first match at-or-after anchor.
                    int target = 0;
                    for (int i = 0; i < (int)search_matches_.size(); ++i) {
                        if ((int)search_matches_[i] >= search_anchor_line_) { target = i; break; }
                    }
                    search_match_idx_ = target;
                } else {
                    search_match_idx_ = (search_match_idx_ + 1) % (int)search_matches_.size();
                }
                cursor_ = (int)search_matches_[search_match_idx_];
            }
            return;
        }
        if (c == 0x10 /*Ctrl-P*/) {
            if (!search_matches_.empty()) {
                if (search_match_idx_ < 0) search_match_idx_ = (int)search_matches_.size() - 1;
                else search_match_idx_ = (search_match_idx_ - 1 + (int)search_matches_.size())
                                          % (int)search_matches_.size();
                cursor_ = (int)search_matches_[search_match_idx_];
            }
            return;
        }
        if (c == 0x7F /*Backspace*/ || c == 0x08 /*also BS*/) {
            if (!search_query_.empty()) search_query_.pop_back();
            search_matches_ = buffer_.findAll(search_query_);
            search_match_idx_ = -1;
            return;
        }
        if (c >= 0x20 && c < 0x7F) {
            search_query_.push_back((char)c);
            search_matches_ = buffer_.findAll(search_query_);
            search_match_idx_ = -1;
            return;
        }
        // Unhandled while search active: swallow.
        return;
    }

    if (evt.source == Source::Keyboard && evt.type == EventType::Keypress
        && evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop(); return;
    }

    // Ctrl-F: enter search mode (Text only).
    if (buffer_.mode() == fb::FileBuffer::Mode::Text
        && evt.source == Source::Keyboard && evt.type == EventType::Keypress
        && evt.data_length == 1 && evt.data[0] == 0x06 /*Ctrl-F*/) {
        search_active_ = true;
        search_query_.clear();
        search_matches_.clear();
        search_match_idx_ = -1;
        search_anchor_line_ = cursor_;
        return;
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

    if ((buffer_.mode() == fb::FileBuffer::Mode::Hex
         || buffer_.mode() == fb::FileBuffer::Mode::TooLarge)
        && evt.source == Source::Keyboard && evt.type == EventType::Keypress) {
        int total = (buffer_.mode() == fb::FileBuffer::Mode::Hex)
            ? (int)((buffer_.size() + 15) / 16)
            : (int)(2 * (32*1024/16) + 1);
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
    if (search_active_) {
        char bar[80];
        int n = (int)search_matches_.size();
        std::snprintf(bar, sizeof(bar), "Find: %s (%d/%d)   [Esc]",
                      search_query_.c_str(),
                      n == 0 ? 0 : (search_match_idx_ + 1),
                      n);
        onebit::drawBitmapText(fb, font, 2, fh + 2, bar, onebit::BLACK);
    } else {
        onebit::drawBitmapText(fb, font, 2, fh + 2,
                               "up/dn line  Sh+up/dn page  Ctrl-F find  Esc back",
                               onebit::BLACK);
    }
}
void FileViewerScreen::renderHex(onebit::IFramebuffer& fb,
                                 const onebit::BitmapFont& font) {
    int rows = std::max(1, (fb.height() - 24) / 10);
    int total_rows = (int)((buffer_.size() + 15) / 16);
    if (cursor_ < scroll_top_) scroll_top_ = cursor_;
    if (cursor_ >= scroll_top_ + rows) scroll_top_ = cursor_ - rows + 1;

    char head[80];
    std::snprintf(head, sizeof(head), "%s   offset 0x%04X",
                  path_.c_str(), (unsigned)cursor_ * 16);
    onebit::drawBitmapText(fb, font, 2, 1, head, onebit::BLACK);
    onebit::fillRect(fb, 0, 11, fb.width(), 1, onebit::BLACK);

    int y = 13;
    char rowbuf[80];
    for (int i = 0; i < rows; ++i) {
        int idx = scroll_top_ + i;
        if (idx >= total_rows) break;
        std::uint32_t off = (std::uint32_t)idx * 16;
        fb::FileBuffer::formatHexRow(buffer_.data(), buffer_.size(), off, rowbuf);
        bool sel = (idx == cursor_);
        if (sel) onebit::fillRect(fb, 0, y, fb.width(), 10, onebit::BLACK);
        onebit::drawBitmapText(fb, font, 2, y + 1, rowbuf,
                               sel ? onebit::WHITE : onebit::BLACK);
        y += 10;
    }

    int fh = fb.height() - 12;
    onebit::fillRect(fb, 0, fh, fb.width(), 1, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 2, fh + 2,
                           "up/dn row  Sh+up/dn page  Ctrl-G goto  Esc back",
                           onebit::BLACK);
}

void FileViewerScreen::renderTooLarge(onebit::IFramebuffer& fb,
                                      const onebit::BitmapFont& font) {
    constexpr std::size_t kChunk = 32 * 1024;
    int head_rows = (int)(kChunk / 16);
    int banner_row = head_rows;
    int tail_rows = (int)(kChunk / 16);
    int total_rows = head_rows + 1 + tail_rows;

    int rows = std::max(1, (fb.height() - 24) / 10);
    if (cursor_ < 0) cursor_ = 0;
    if (cursor_ >= total_rows) cursor_ = total_rows - 1;
    if (cursor_ < scroll_top_) scroll_top_ = cursor_;
    if (cursor_ >= scroll_top_ + rows) scroll_top_ = cursor_ - rows + 1;

    char head[80];
    std::snprintf(head, sizeof(head), "%s   too large -- head/tail only",
                  path_.c_str());
    onebit::drawBitmapText(fb, font, 2, 1, head, onebit::BLACK);
    onebit::fillRect(fb, 0, 11, fb.width(), 1, onebit::BLACK);

    // For tail rows we want the *absolute* file offset, not the in-buffer
    // offset. Compute it from the real file size (FileBuffer tracks this).
    std::size_t file_size = buffer_.fileSize();
    std::uint32_t tail_abs_base = (std::uint32_t)(file_size > kChunk ? file_size - kChunk : 0);

    int y = 13;
    char rowbuf[80];
    for (int i = 0; i < rows; ++i) {
        int idx = scroll_top_ + i;
        if (idx >= total_rows) break;
        bool sel = (idx == cursor_);
        if (sel) onebit::fillRect(fb, 0, y, fb.width(), 10, onebit::BLACK);
        if (idx == banner_row) {
            onebit::drawBitmapText(fb, font, 2, y + 1,
                "... [too large; tail follows] ...",
                sel ? onebit::WHITE : onebit::BLACK);
        } else if (idx < head_rows) {
            fb::FileBuffer::formatHexRow(buffer_.data(), buffer_.size(),
                                         (std::uint32_t)idx * 16, rowbuf);
            onebit::drawBitmapText(fb, font, 2, y + 1, rowbuf,
                                   sel ? onebit::WHITE : onebit::BLACK);
        } else {
            // Tail row k of tail_rows lives in the buffer at kChunk + k*16,
            // and corresponds to absolute file offset tail_abs_base + k*16.
            // formatHexRow renders the address column from the offset arg, so
            // we render from a temp buffer that starts at the tail's slice
            // with the absolute offset as the displayed address.
            int k = idx - banner_row - 1;
            // Build a temp buffer view: copy 16 bytes from buffer_'s tail
            // region into a small stack buffer addressed at tail_abs_base+k*16.
            char slice[16];
            std::size_t buf_off = kChunk + (std::size_t)k * 16;
            std::size_t avail = (buf_off + 16 <= buffer_.size())
                                ? 16
                                : (buffer_.size() > buf_off ? buffer_.size() - buf_off : 0);
            std::memcpy(slice, buffer_.data() + buf_off, avail);
            // Pass the slice with a synthesized base/offset such that
            // formatHexRow's address column shows tail_abs_base + k*16.
            // formatHexRow indexes base[offset+i]; passing base = slice -
            // (tail_abs_base + k*16) lets `slice[i]` line up with index
            // `tail_abs_base + k*16 + i`. base_size is set so the same i's
            // are valid.
            std::uint32_t abs_off = tail_abs_base + (std::uint32_t)k * 16;
            const char* virt_base = slice - abs_off;
            std::size_t virt_size = (std::size_t)abs_off + avail;
            fb::FileBuffer::formatHexRow(virt_base, virt_size, abs_off, rowbuf);
            onebit::drawBitmapText(fb, font, 2, y + 1, rowbuf,
                                   sel ? onebit::WHITE : onebit::BLACK);
        }
        y += 10;
    }
}

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

namespace {
constexpr Command kViewerTextContextual[] = {
    { "Find",            "", 0xFF60 },
    { "Top of file",     "", 0xFF62 },
    { "Bottom of file",  "", 0xFF63 },
    { "Edit",            "", 0xFF64 },
};
constexpr Command kViewerHexContextual[] = {
    { "Go to offset...", "", 0xFF61 },
    { "Top of file",     "", 0xFF62 },
    { "Bottom of file",  "", 0xFF63 },
};
}  // namespace

SpanView<const KeybindHint> FileViewerScreen::keybindHints() const { return {}; }

SpanView<const Command> FileViewerScreen::getContextualCommands() {
    if (buffer_.mode() == fb::FileBuffer::Mode::Text) {
        return SpanView<const Command>(kViewerTextContextual, 4);
    }
    if (buffer_.mode() == fb::FileBuffer::Mode::Hex
        || buffer_.mode() == fb::FileBuffer::Mode::TooLarge) {
        return SpanView<const Command>(kViewerHexContextual, 3);
    }
    return {};
}

void FileViewerScreen::dispatchContextual(uint16_t id) {
    switch (id) {
        case 0xFF60: {  // Find
            if (buffer_.mode() != fb::FileBuffer::Mode::Text) return;
            search_active_ = true;
            search_query_.clear();
            search_matches_.clear();
            search_match_idx_ = -1;
            search_anchor_line_ = cursor_;
            return;
        }
        case 0xFF61: {  // Goto offset
            ctx_.stack.push(std::make_unique<TextInputScreen>(ctx_,
                "Go to offset (hex or dec)", "",
                [this](TextInputResult r, const std::string& v) {
                    if (r != TextInputResult::Submit || v.empty()) return;
                    std::uint64_t off = 0;
                    int base = 10;
                    std::string s = v;
                    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                        base = 16; s = s.substr(2);
                    }
                    char* end = nullptr;
                    off = std::strtoull(s.c_str(), &end, base);
                    if (end == s.c_str()) {
                        ctx_.overlay.showError("Bad offset", "Hex (0xNN) or decimal");
                        return;
                    }
                    cursor_ = (int)(off / 16);
                }));
            return;
        }
        case 0xFF62: cursor_ = 0; return;
        case 0xFF63: {
            int total = (buffer_.mode() == fb::FileBuffer::Mode::Text)
                ? (int)buffer_.lineCount()
                : (int)((buffer_.size() + 15) / 16);
            cursor_ = std::max(0, total - 1);
            return;
        }
        case 0xFF64: {  // Edit
            if (buffer_.mode() != fb::FileBuffer::Mode::Text) return;
            auto path = path_;
            ctx_.stack.replace(std::make_unique<EditorScreen>(ctx_, path, /*new_file=*/false));
            return;
        }
    }
}

}  // namespace app
