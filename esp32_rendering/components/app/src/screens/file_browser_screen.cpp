#include "screens/file_browser_screen.hpp"
#include "screen_stack.hpp"
#include "screens/file_viewer_screen.hpp"
#include "screens/command_palette.hpp"
#include "overlay.hpp"
#include "command_ids.hpp"
#include "sdcard_manager.hpp"
#include "path_helpers.hpp"
#include "command_registry.hpp"
#include "screens/text_input_screen.hpp"
#include <memory>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

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

void FileBrowserScreen::handleInput(const input::InputEvent& evt,
                                    ScreenStack& stack) {
    using namespace input;

    if (state_ == State::NoSdcard) {
        if (evt.source == Source::Keyboard && evt.type == EventType::Keypress) {
            if (evt.data_length >= 3 && evt.data[0] == 0x1B && evt.data[1] == 0x5B) {
                if (evt.data[2] == 'D') err_focus_ = 0;
                else if (evt.data[2] == 'C') err_focus_ = 1;
                return;
            }
            if (evt.data_length == 1 && (evt.data[0] == '\r' || evt.data[0] == '\n')) {
                if (err_focus_ == 0) onEnter();
                else stack.pop();
                return;
            }
            if (evt.data_length == 1 && evt.data[0] == 0x1B) {
                stack.pop();
                return;
            }
        }
        if (evt.source == Source::Button && evt.type == EventType::ButtonShort) {
            if (evt.button_id == 0) onEnter();   // A = retry
            else stack.pop();                    // B = back
            return;
        }
        return;
    }

    if (evt.source == Source::Button) {
        if (evt.type == EventType::ButtonShort && evt.button_id == 0) {
            enterSelected(stack);
        } else if (evt.type == EventType::ButtonShort && evt.button_id == 1) {
            goUp(stack);
        } else if (evt.type == EventType::ButtonLong && evt.button_id == 0) {
            ctx_.stack.push(std::make_unique<CommandPalette>(ctx_));
        }
        return;
    }
    if (evt.source != Source::Keyboard || evt.type != EventType::Keypress) return;

    auto total = (int)listing_.entries().size();
    auto move = [&](int delta) {
        if (total == 0) return;
        selected_ = (selected_ + delta + total) % total;
    };

    // ESC sequences first (longer matches before shorter).
    if (evt.data_length >= 6 && evt.data[0] == 0x1B && evt.data[1] == 0x5B
        && evt.data[2] == 0x31 && evt.data[3] == 0x3B && evt.data[4] == 0x32) {
        // Shift-arrow modifier sequence.
        int rows = visibleRows(ctx_.fb.height()) - 2;
        if (rows < 1) rows = 1;
        if (evt.data[5] == 'A') move(-rows);
        else if (evt.data[5] == 'B') move(rows);
        return;
    }
    if (evt.data_length >= 3 && evt.data[0] == 0x1B && evt.data[1] == 0x5B) {
        switch (evt.data[2]) {
            case 'A': move(-1); return;
            case 'B': move(1); return;
            case 'D': goUp(stack); return;  // Left arrow
            case 'C': enterSelected(stack); return;  // Right arrow (alt-Enter)
        }
    }
    if (evt.data_length == 1) {
        switch (evt.data[0]) {
            case 0x1B: goUp(stack); return;             // Esc
            case '\r': case '\n': enterSelected(stack); return;
            case 0x08:                                   // Ctrl-H
                hidden_ = (hidden_ == fb::DirListing::HiddenMode::Hide)
                    ? fb::DirListing::HiddenMode::Show
                    : fb::DirListing::HiddenMode::Hide;
                reload();
                return;
            case 0x0B:                                   // Ctrl-K palette
                ctx_.stack.push(std::make_unique<CommandPalette>(ctx_));
                return;
        }
    }
}

void FileBrowserScreen::enterSelected(ScreenStack& stack) {
    if (selected_ < 0 || selected_ >= (int)listing_.entries().size()) return;
    const auto& e = listing_.entries()[selected_];
    if (!e.bookmark_target.empty()) {
        // Bookmark.
        if (e.is_dir) {
            nav_history_.push_back(current_path_);
            current_path_ = e.bookmark_target;
            selected_ = 0; scroll_top_ = 0;
            reload();
        } else {
            stack.push(std::make_unique<FileViewerScreen>(ctx_, e.bookmark_target));
        }
        return;
    }
    std::string full = fb::path::join(current_path_, e.name);
    if (e.is_dir) {
        nav_history_.push_back(current_path_);
        current_path_ = full;
        selected_ = 0; scroll_top_ = 0;
        reload();
    } else {
        stack.push(std::make_unique<FileViewerScreen>(ctx_, full));
    }
}

void FileBrowserScreen::goUp(ScreenStack& stack) {
    if (current_path_ == "/sdcard") {
        stack.pop();
        return;
    }
    if (!nav_history_.empty()) {
        current_path_ = std::move(nav_history_.back());
        nav_history_.pop_back();
    } else {
        current_path_ = fb::path::parent(current_path_);
    }
    selected_ = 0; scroll_top_ = 0;
    reload();
}

void FileBrowserScreen::runDelete(const std::string& full_path, bool is_dir) {
    int rc;
    if (is_dir) {
        // Pre-check: empty?
        DIR* d = opendir(full_path.c_str());
        if (d) {
            int n = 0;
            struct dirent* de;
            while ((de = readdir(d)) != nullptr) {
                if (std::strcmp(de->d_name, ".") == 0
                    || std::strcmp(de->d_name, "..") == 0) continue;
                ++n; if (n > 0) break;
            }
            closedir(d);
            if (n > 0) { ctx_.overlay.showError("Cannot delete", "Directory not empty"); return; }
        }
        rc = ::rmdir(full_path.c_str());
    } else {
        rc = ::unlink(full_path.c_str());
    }
    if (rc != 0) {
        last_failed_op_ = [this, full_path, is_dir]() { runDelete(full_path, is_dir); };
        showMidOpError(is_dir ? "rmdir" : "unlink", errno);
        return;
    }
    ESP_LOGI(TAG, "deleted %s", full_path.c_str());
    reload();
}

void FileBrowserScreen::runRename(const std::string& old_full,
                                  const std::string& new_name) {
    std::string parent = fb::path::parent(old_full);
    std::string new_full = fb::path::join(parent, new_name);
    if (::rename(old_full.c_str(), new_full.c_str()) != 0) {
        last_failed_op_ = [this, old_full, new_name]() { runRename(old_full, new_name); };
        showMidOpError("rename", errno);
        return;
    }
    ESP_LOGI(TAG, "renamed %s -> %s", old_full.c_str(), new_full.c_str());
    reload();
    // Place cursor on the renamed entry.
    for (int i = 0; i < (int)listing_.entries().size(); ++i) {
        if (listing_.entries()[i].name == new_name) { selected_ = i; break; }
    }
}

void FileBrowserScreen::runMkdir(const std::string& parent,
                                 const std::string& new_name) {
    std::string full = fb::path::join(parent, new_name);
    struct stat st{};
    if (::stat(full.c_str(), &st) == 0) {
        ctx_.overlay.showError("Cannot create", "Already exists");
        return;
    }
    if (::mkdir(full.c_str(), 0755) != 0) {
        last_failed_op_ = [this, parent, new_name]() { runMkdir(parent, new_name); };
        showMidOpError("mkdir", errno);
        return;
    }
    ESP_LOGI(TAG, "mkdir %s", full.c_str());
    reload();
    for (int i = 0; i < (int)listing_.entries().size(); ++i) {
        if (listing_.entries()[i].name == new_name) { selected_ = i; break; }
    }
}

void FileBrowserScreen::showMidOpError(const char* op, int err_code) {
    char body[120];
    std::snprintf(body, sizeof(body), "%s: %s\nRetry? (No -> /sdcard)", op, std::strerror(err_code));
    ctx_.overlay.showConfirm("SD card error", body,
        [this](bool yes) {
            if (yes && last_failed_op_) {
                auto op = std::move(last_failed_op_);
                last_failed_op_ = nullptr;
                op();
            } else {
                last_failed_op_ = nullptr;
                nav_history_.clear();
                current_path_ = "/sdcard";
                if (!ensureMounted()) {
                    state_ = State::NoSdcard;
                } else {
                    selected_ = 0; scroll_top_ = 0;
                    reload();
                }
            }
        });
}

const char* FileBrowserScreen::validateName(const std::string& n) {
    if (n.empty()) return "Name required";
    if (n == "." || n == "..") return "Reserved name";
    if (n.find('/') != std::string::npos) return "Slashes not allowed";
    if (n.size() > 255) return "Name too long";
    return nullptr;
}

namespace {
constexpr Command kBrowserContextual[] = {
    { "Delete current entry", "", 0xFF50 },
    { "Rename current entry", "", 0xFF51 },
    { "New folder...",        "", 0xFF52 },
    { "Show/Hide hidden",     "", 0xFF53 },
};
}  // namespace

SpanView<const KeybindHint> FileBrowserScreen::keybindHints() const { return {}; }

SpanView<const Command> FileBrowserScreen::getContextualCommands() {
    return SpanView<const Command>(kBrowserContextual, 4);
}

void FileBrowserScreen::dispatchContextual(uint16_t id) {
    if (selected_ < 0 || selected_ >= (int)listing_.entries().size()) return;
    const auto& e = listing_.entries()[selected_];

    switch (id) {
        case 0xFF50: {  // Delete
            if (!e.bookmark_target.empty()) {
                ctx_.overlay.showError("Cannot modify", "Bookmark row");
                return;
            }
            std::string full = fb::path::join(current_path_, e.name);
            char body[80];
            char sz[16]; formatSize(e.size, sz);
            std::snprintf(body, sizeof(body), "Delete '%s' (%s)?",
                          e.name.c_str(), e.is_dir ? "folder" : sz);
            bool was_dir = e.is_dir;
            ctx_.overlay.showConfirm("Confirm delete", body,
                [this, full, was_dir](bool yes) {
                    if (!yes) return;
                    runDelete(full, was_dir);
                });
            return;
        }
        case 0xFF53: {  // Toggle hidden
            hidden_ = (hidden_ == fb::DirListing::HiddenMode::Hide)
                ? fb::DirListing::HiddenMode::Show
                : fb::DirListing::HiddenMode::Hide;
            reload();
            return;
        }
        case 0xFF51: {  // Rename
            if (!e.bookmark_target.empty()) {
                ctx_.overlay.showError("Cannot modify", "Bookmark row");
                return;
            }
            std::string full = fb::path::join(current_path_, e.name);
            std::string old_name = e.name;
            ctx_.stack.push(std::make_unique<TextInputScreen>(ctx_,
                "Rename", e.name.c_str(),
                [this, full, old_name](TextInputResult r, const std::string& v) {
                    if (r != TextInputResult::Submit) return;
                    if (v == old_name) return;
                    if (auto err = validateName(v)) {
                        ctx_.overlay.showError("Invalid name", err);
                        return;
                    }
                    runRename(full, v);
                }));
            return;
        }
        case 0xFF52: {  // Mkdir
            std::string parent = current_path_;
            ctx_.stack.push(std::make_unique<TextInputScreen>(ctx_,
                "New folder", "",
                [this, parent](TextInputResult r, const std::string& v) {
                    if (r != TextInputResult::Submit) return;
                    if (auto err = validateName(v)) {
                        ctx_.overlay.showError("Invalid name", err);
                        return;
                    }
                    runMkdir(parent, v);
                }));
            return;
        }
        default: return;
    }
}

}  // namespace app
