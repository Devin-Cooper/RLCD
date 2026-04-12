#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <cstdint>

namespace app {

/// Lightweight overlay menu rendered on top of the current mode.
/// Navigation via arrow keys (keyboard) or physical buttons.
class Menu {
public:
    /// Menu item identifiers
    enum class Item : uint8_t {
        Dashboard = 0,
        Terminal,
        Settings,
        WiFi,
        About,
        Count
    };

    Menu();

    /// Show/hide the menu overlay.
    void open();
    void close();
    bool isOpen() const { return visible_; }

    /// Navigate: move selection up/down, confirm selection.
    void moveUp();
    void moveDown();
    Item confirm();

    /// Get currently highlighted item.
    Item selected() const { return static_cast<Item>(selected_index_); }

    /// Render the menu overlay onto the framebuffer.
    /// Draws a bordered list with a highlight bar over the selected item.
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);

private:
    bool visible_;
    uint8_t selected_index_;

    static constexpr int ITEM_COUNT = static_cast<int>(Item::Count);

    /// Menu item labels (compile-time)
    static const char* itemLabel(int index);
};

} // namespace app
