#include "screens/file_browser_screen.hpp"
#include "screen_stack.hpp"
#include "screens/file_viewer_screen.hpp"
#include "overlay.hpp"
#include "command_ids.hpp"
#include "sdcard_manager.hpp"

#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/core/framebuffer.hpp>

#include <esp_log.h>
#include <ctime>
#include <cstdio>
#include <algorithm>

namespace app {

static const char* TAG = "file_browser";

FileBrowserScreen::FileBrowserScreen(ScreenContext& ctx) : ctx_(ctx) {}

void FileBrowserScreen::onEnter() {
    if (ensureMounted()) {
        state_ = State::OK;
        reload();
    } else {
        state_ = State::NoSdcard;
        err_focus_ = 0;
    }
}

bool FileBrowserScreen::ensureMounted() {
    if (ctx_.sdcard.isMounted()) return true;
    return ctx_.sdcard.mount();
}

void FileBrowserScreen::reload() {
    listing_.load(current_path_, hidden_);
    if (selected_ >= (int)listing_.entries().size()) {
        selected_ = std::max(0, (int)listing_.entries().size() - 1);
    }
    scroll_top_ = std::min(scroll_top_, selected_);
}

int FileBrowserScreen::visibleRows(int screen_h) const {
    return std::max(1, (screen_h - 32) / 10);
}

void FileBrowserScreen::formatSize(uint64_t b, char out[16]) {
    if (b < 1024) std::snprintf(out, 16, "%u B", (unsigned)b);
    else if (b < 1024u*1024) std::snprintf(out, 16, "%.1f KB", (double)b / 1024);
    else if (b < 1024u*1024*1024) std::snprintf(out, 16, "%.1f MB", (double)b / (1024*1024));
    else std::snprintf(out, 16, "%.1f GB", (double)b / (1024.0*1024*1024));
}

void FileBrowserScreen::formatMtime(time_t mt, char out[16]) {
    if (mt == 0) { std::snprintf(out, 16, "--"); return; }
    struct tm tm{};
    localtime_r(&mt, &tm);
    std::strftime(out, 16, "%m-%d %H:%M", &tm);
}

void FileBrowserScreen::drawRow(onebit::IFramebuffer& fb,
                                const onebit::BitmapFont& font,
                                int y, const fb::FileEntry& e, bool selected) const {
    if (selected) {
        onebit::fillRect(fb, 0, y, fb.width(), 10, onebit::BLACK);
    }
    auto color = selected ? onebit::WHITE : onebit::BLACK;
    char prefix = selected ? '>' : ' ';
    char buf[80];
    if (!e.bookmark_target.empty()) {
        std::snprintf(buf, sizeof(buf), "%c %s", prefix, e.name.c_str());
    } else {
        char sz[16], mt[16];
        formatSize(e.size, sz);
        formatMtime(e.mtime, mt);
        std::snprintf(buf, sizeof(buf), "%c %s%-22s %-7s %s",
                      prefix,
                      e.is_dir ? "/" : " ",
                      e.name.c_str(),
                      e.is_dir ? "-" : sz, mt);
    }
    onebit::drawBitmapText(fb, font, 2, y + 1, buf, color);
}

void FileBrowserScreen::render(onebit::IFramebuffer& fb,
                               const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);

    if (state_ == State::NoSdcard) {
        const char* msg1 = "No SD card";
        const char* msg2 = "Insert an SD card and choose Retry.";
        int cy = fb.height() / 2 - 20;
        onebit::drawBitmapText(fb, font, 4, cy, msg1, onebit::BLACK);
        onebit::drawBitmapText(fb, font, 4, cy + 12, msg2, onebit::BLACK);
        const char* labels[] = { "[Retry]", "[Back]" };
        for (int i = 0; i < 2; ++i) {
            int x = 4 + i * 70;
            int y = cy + 30;
            if (i == err_focus_) {
                onebit::fillRect(fb, x - 2, y - 1, 60, 12, onebit::BLACK);
            }
            onebit::drawBitmapText(fb, font, x, y, labels[i],
                                   i == err_focus_ ? onebit::WHITE : onebit::BLACK);
        }
        return;
    }

    // ---- Breadcrumb header ----
    char head[80];
    int idx = selected_ + 1;
    int total = (int)listing_.entries().size();
    std::snprintf(head, sizeof(head), "%s   %d/%d", current_path_.c_str(), idx, total);
    onebit::drawBitmapText(fb, font, 2, 1, head, onebit::BLACK);
    onebit::fillRect(fb, 0, 11, fb.width(), 1, onebit::BLACK);

    // ---- Listing ----
    int rows = visibleRows(fb.height());
    if (selected_ < scroll_top_) scroll_top_ = selected_;
    if (selected_ >= scroll_top_ + rows) scroll_top_ = selected_ - rows + 1;
    int y = 13;
    for (int i = 0; i < rows; ++i) {
        int row_idx = scroll_top_ + i;
        if (row_idx >= total) break;
        drawRow(fb, font, y, listing_.entries()[row_idx], row_idx == selected_);
        y += 10;
    }

    // ---- Footer ----
    int fh = fb.height() - 12;
    onebit::fillRect(fb, 0, fh, fb.width(), 1, onebit::BLACK);
    onebit::drawBitmapText(fb, font, 2, fh + 2,
        "up/dn move  Enter open  Esc up  Ctrl-K palette",
        onebit::BLACK);
}

void FileBrowserScreen::handleInput(const input::InputEvent& /*evt*/,
                                    ScreenStack& /*stack*/) {
    // Implemented in Task 10.
}

void FileBrowserScreen::enterSelected(ScreenStack& /*stack*/) {
    // Task 10.
}

void FileBrowserScreen::goUp(ScreenStack& /*stack*/) {
    // Task 10.
}

void FileBrowserScreen::runDelete(const std::string& /*full_path*/, bool /*is_dir*/) {
    // Task 12.
}

void FileBrowserScreen::runRename(const std::string& /*old_full*/, const std::string& /*new_name*/) {
    // Task 13.
}

void FileBrowserScreen::runMkdir(const std::string& /*parent*/, const std::string& /*new_name*/) {
    // Task 13.
}

void FileBrowserScreen::showMidOpError(const char*, int) {
    // Task 14.
    ctx_.overlay.showError("SD error", "(retry path: Task 14)");
}

const char* FileBrowserScreen::validateName(const std::string& /*n*/) {
    // Task 13.
    return nullptr;
}

SpanView<const KeybindHint> FileBrowserScreen::keybindHints() const { return {}; }
SpanView<const Command> FileBrowserScreen::getContextualCommands() { return {}; }
void FileBrowserScreen::dispatchContextual(uint16_t /*id*/) {}

}  // namespace app
