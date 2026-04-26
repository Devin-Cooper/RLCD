#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "command_registry.hpp"
#include "text_input.hpp"
#include <vector>

namespace app {

class CommandPalette : public Screen {
public:
    explicit CommandPalette(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    bool isTransparent() const override { return true; }
    bool wantsKeybindFooter() const override { return false; }
    bool bypassesKeyboardGate() const override { return true; }
    ScreenKind screenKind() const override { return ScreenKind::CommandPalette; }
    const char* breadcrumbLabel() const override { return "Palette"; }

private:
    ScreenContext& ctx_;
    char           query_buf_[33] = {};
    TextInput      query_;
    std::vector<Command> filtered_;
    int            selected_ = 0;
    int16_t        prev_selected_y_ = 0;
    bool           focus_y_initialized_ = false;
    int16_t        list_start_y_ = 0;
    int16_t        row_h_ = 0;

    void rebuildFiltered();
    int16_t computeRowY(int idx) const;
};

} // namespace app
