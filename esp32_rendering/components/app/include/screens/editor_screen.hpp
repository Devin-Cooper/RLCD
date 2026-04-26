#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "file_buffer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app {

class EditorScreen : public Screen {
public:
    EditorScreen(ScreenContext& ctx, std::string path, bool new_file);
    ~EditorScreen();

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    SpanView<const KeybindHint> keybindHints() const override;
    SpanView<const Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;
    const char* breadcrumbLabel() const override { return breadcrumb_; }

    bool bypassesKeyboardGate() const override { return false; }   // mutation requires gate

private:
    ScreenContext& ctx_;
    std::string path_;
    bool        new_file_ = false;
    char        breadcrumb_[64]{};
    fb::FileBuffer buffer_;

    // Cursor (logical, byte-granular within line).
    int line_     = 0;
    int byte_col_ = 0;
    int sticky_display_col_ = 0;   // remembered display col for vertical moves

    // Horizontal scroll: leftmost displayed display-column.
    int hscroll_ = 0;

    // Selection.
    int anchor_line_     = -1;
    int anchor_byte_col_ = 0;

    // Find / replace.
    enum class FindMode { Off, Find, Replace };
    FindMode    find_mode_ = FindMode::Off;
    std::string find_query_;
    std::string replace_with_;
    std::vector<std::uint32_t> find_matches_;
    int find_match_idx_ = -1;
    int find_anchor_line_ = 0;

    // Clipboard (PSRAM, lazy-allocated, 16 KB cap).
    static constexpr std::size_t kClipCapBytes = 16u * 1024u;
    char* clipboard_ = nullptr;
    std::size_t clipboard_size_ = 0;

    // Layout cache.
    int viewport_rows_ = 0;
    int scroll_top_ = 0;

    // State machine.
    enum class State { OK, BadMode };
    State state_ = State::OK;

    // Helpers (impls in subsequent tasks).
    void updateBreadcrumb();
    bool ensureSnapshot();
    void invalidateSelection();
    bool hasSelection() const { return anchor_line_ >= 0; }
    void normalizeSelection(int& a_l, int& a_c, int& b_l, int& b_c) const;
    std::size_t cursorByteOffset() const;
    void cursorFromByteOffset(std::size_t pos);
    int  displayColForByteCol(int line, int byte_col) const;
    void ensureCursorVisible();

    void onPrintable(char c);
    void onEnterKey();
    void onBackspace();
    void onDelete();
    void onMove(int dline, int dcol, bool extend);
    void onCopy();
    void onCut();
    void onPaste();
    void onUndo();
    void onSave(bool save_as, ScreenStack& stack);
    void doSaveAtomic(const std::string& dest);

    void enterFindMode();
    void enterReplaceMode();
    void doReplaceCurrent();

    void onEscape(ScreenStack& stack);

    void renderEditor(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);
    void renderFindBar(onebit::IFramebuffer& fb, const onebit::BitmapFont& font, int y);
    void renderRow(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                   int y, int line, bool is_cursor_line) const;
};

}  // namespace app
