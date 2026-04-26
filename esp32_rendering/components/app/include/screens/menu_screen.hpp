#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "animator.hpp"
#include "command_ids.hpp"

namespace app {

class MenuScreen : public Screen {
public:
    enum class Item : uint8_t {
        Dashboard = 0,
        Terminal,
        Servers,
        SshKeys,
        Settings,
        Audio,
        WiFi,
        About,
        Count,
    };

    explicit MenuScreen(ScreenContext& ctx);

    bool isTransparent() const override { return true; }
    int targetFps() const override { return 10; }

    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

    Item selected() const { return static_cast<Item>(selected_index_); }

private:
    ScreenContext& ctx_;
    uint8_t selected_index_ = 0;
    static constexpr int ITEM_COUNT = static_cast<int>(Item::Count);
    static const char* itemLabel(int index);
    void dispatchSelection(ScreenStack& stack, Item item);

    // Phase 5: focus-rect animation
    int16_t prev_selected_y_ = 0;
    bool    focus_y_initialized_ = false;
    // Cached layout bounds for focus rect; refreshed each render before any
    // mid-render selection -> y math is needed.
    int16_t menu_x_ = 0;
    int16_t menu_w_ = 0;
    int16_t cell_h_ = 0;
    int16_t first_row_y_ = 0;
    int16_t computeRowY(int index) const;
    void    onSelectionChange(uint8_t old_index, uint8_t new_index);
};

} // namespace app
