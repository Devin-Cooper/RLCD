#include "menu.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <cstring>

namespace app {

// ============================================================================
// Item labels
// ============================================================================

const char* Menu::itemLabel(int index) {
    switch (static_cast<Item>(index)) {
        case Item::Dashboard: return "Dashboard";
        case Item::Terminal:  return "Terminal";
        case Item::Settings:  return "Settings";
        case Item::WiFi:      return "WiFi";
        case Item::About:     return "About";
        default:              return "?";
    }
}

// ============================================================================
// Construction / state
// ============================================================================

Menu::Menu()
    : visible_(false)
    , selected_index_(0)
{}

void Menu::open() {
    visible_ = true;
    selected_index_ = 0;
}

void Menu::close() {
    visible_ = false;
}

void Menu::moveUp() {
    if (selected_index_ > 0) {
        --selected_index_;
    }
}

void Menu::moveDown() {
    if (selected_index_ < ITEM_COUNT - 1) {
        ++selected_index_;
    }
}

Menu::Item Menu::confirm() {
    return static_cast<Item>(selected_index_);
}

// ============================================================================
// Rendering
// ============================================================================

void Menu::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    if (!visible_) return;

    const int16_t cell_w = font.glyph_width + 1;  // +1 for spacing
    const int16_t cell_h = font.glyph_height + 2;  // +2 for row padding

    // Menu dimensions
    const int16_t menu_w = cell_w * 14 + 8;  // 14 chars max label + padding
    const int16_t menu_h = cell_h * ITEM_COUNT + 8;  // items + border padding

    // Center on screen
    const int16_t menu_x = (fb.width() - menu_w) / 2;
    const int16_t menu_y = (fb.height() - menu_h) / 2;

    // Clear background
    onebit::fillRect(fb, menu_x, menu_y, menu_w, menu_h, onebit::WHITE);

    // Draw double border
    onebit::drawRect(fb, menu_x, menu_y, menu_w, menu_h, onebit::BLACK);
    onebit::drawRect(fb, menu_x + 2, menu_y + 2, menu_w - 4, menu_h - 4, onebit::BLACK);

    // Draw items
    const int16_t text_x = menu_x + 6;
    int16_t text_y = menu_y + 5;

    for (int i = 0; i < ITEM_COUNT; ++i) {
        const char* label = itemLabel(i);

        if (i == selected_index_) {
            // Highlight bar (inverted)
            onebit::fillRect(fb,
                             menu_x + 3, text_y - 1,
                             menu_w - 6, cell_h,
                             onebit::BLACK);
            onebit::drawBitmapText(fb, font, text_x, text_y, label,
                                   onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, text_x, text_y, label,
                                   onebit::BLACK);
        }

        text_y += cell_h;
    }
}

} // namespace app
