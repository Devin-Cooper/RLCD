#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "file_buffer.hpp"

#include <string>
#include <vector>

namespace app {

class FileViewerScreen : public Screen {
public:
    FileViewerScreen(ScreenContext& ctx, std::string path);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;
    SpanView<const KeybindHint> keybindHints() const override;
    SpanView<const Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;
    const char* breadcrumbLabel() const override { return breadcrumb_; }

private:
    ScreenContext& ctx_;
    std::string path_;
    char breadcrumb_[64];
    fb::FileBuffer buffer_;

    // Common cursor: line number (Text) or row index (Hex/TooLarge — 16 B/row).
    int cursor_ = 0;
    int scroll_top_ = 0;

    // Search state (Text mode only).
    bool search_active_ = false;
    std::string search_query_;
    std::vector<std::uint32_t> search_matches_;
    int search_match_idx_ = -1;
    int search_anchor_line_ = 0;

    void renderText(onebit::IFramebuffer&, const onebit::BitmapFont&);
    void renderHex(onebit::IFramebuffer&, const onebit::BitmapFont&);
    void renderTooLarge(onebit::IFramebuffer&, const onebit::BitmapFont&);
    void renderError(onebit::IFramebuffer&, const onebit::BitmapFont&);
};

}  // namespace app
