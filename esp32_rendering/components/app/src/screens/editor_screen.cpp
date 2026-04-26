#include "screens/editor_screen.hpp"
#include "overlay.hpp"
#include "screen_stack.hpp"

#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/core/framebuffer.hpp>

#include <esp_log.h>
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

void EditorScreen::renderEditor(onebit::IFramebuffer&, const onebit::BitmapFont&) {
    // Filled in Task 6.
}
void EditorScreen::renderFindBar(onebit::IFramebuffer&, const onebit::BitmapFont&, int) {}
void EditorScreen::renderRow(onebit::IFramebuffer&, const onebit::BitmapFont&,
                              int, int, bool) const {}

SpanView<const KeybindHint> EditorScreen::keybindHints() const { return {}; }
SpanView<const Command> EditorScreen::getContextualCommands() { return {}; }
void EditorScreen::dispatchContextual(uint16_t /*id*/) {}

// Stubs filled in by later tasks:
void EditorScreen::invalidateSelection() { anchor_line_ = -1; anchor_byte_col_ = 0; }
void EditorScreen::normalizeSelection(int&, int&, int&, int&) const {}
std::size_t EditorScreen::cursorByteOffset() const { return 0; }
void EditorScreen::cursorFromByteOffset(std::size_t) {}
int EditorScreen::displayColForByteCol(int, int) const { return 0; }
void EditorScreen::ensureCursorVisible() {}
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
