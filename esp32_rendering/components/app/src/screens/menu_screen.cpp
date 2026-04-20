#include "screens/menu_screen.hpp"
#include "screens/dashboard_screen.hpp"
#include "screens/terminal_screen.hpp"
#include "screens/about_screen.hpp"
#include "screens/wifi_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include <1bit/render/primitives.hpp>
#include <esp_log.h>

static const char* TAG = "menu_screen";

namespace app {

const char* MenuScreen::itemLabel(int index) {
    switch (static_cast<Item>(index)) {
        case Item::Dashboard: return "Dashboard";
        case Item::Terminal:  return "Terminal";
        case Item::Servers:   return "Servers";
        case Item::Settings:  return "Settings";
        case Item::WiFi:      return "WiFi";
        case Item::About:     return "About";
        default:              return "?";
    }
}

MenuScreen::MenuScreen(ScreenContext& ctx) : ctx_(ctx) {}

void MenuScreen::handleInput(const input::InputEvent& evt,
                             ScreenStack& stack) {
    // Btn A short — close (pop self)
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 0) {
        stack.pop();
        return;
    }
    // Btn B short — move down
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort &&
        evt.button_id == 1) {
        selected_index_ = (selected_index_ + 1) % ITEM_COUNT;
        return;
    }
    // Btn B long — confirm
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 1) {
        dispatchSelection(stack, selected());
        return;
    }
    // Keyboard
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        // Arrows
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            if (evt.data[2] == 'A')
                selected_index_ = (selected_index_ == 0)
                    ? ITEM_COUNT - 1 : selected_index_ - 1;
            if (evt.data[2] == 'B')
                selected_index_ = (selected_index_ + 1) % ITEM_COUNT;
            return;
        }
        // Enter confirms
        if (evt.data_length == 1 && evt.data[0] == '\r') {
            dispatchSelection(stack, selected());
            return;
        }
        // Esc closes
        if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            stack.pop();
            return;
        }
    }
}

void MenuScreen::dispatchSelection(ScreenStack& stack, Item item) {
    stack.pop();   // pop self first
    switch (item) {
        case Item::Dashboard:
            stack.replace(std::make_unique<DashboardScreen>(ctx_));
            break;
        case Item::Terminal:
            stack.replace(std::make_unique<TerminalScreen>(ctx_));
            break;
        case Item::Servers:
            ESP_LOGI(TAG, "Servers — ServerListScreen deferred to Task 21");
            ctx_.overlay.showToast("Servers editor: coming soon", 2000);
            break;
        case Item::Settings:
            ESP_LOGI(TAG, "Settings — SettingsScreen deferred to Task 23");
            ctx_.overlay.showToast("Settings editor: coming soon", 2000);
            break;
        case Item::WiFi:
            stack.push(std::make_unique<WifiScreen>(ctx_));
            break;
        case Item::About:
            stack.push(std::make_unique<AboutScreen>(ctx_));
            break;
        default:
            break;
    }
}

void MenuScreen::render(onebit::IFramebuffer& fb,
                        const onebit::BitmapFont& font) {
    const int16_t cell_w = font.glyph_width + 1;
    const int16_t cell_h = font.glyph_height + 2;
    const int16_t menu_w = cell_w * 14 + 8;
    const int16_t menu_h = cell_h * ITEM_COUNT + 8;
    const int16_t menu_x = (fb.width()  - menu_w) / 2;
    const int16_t menu_y = (fb.height() - menu_h) / 2;

    onebit::fillRect(fb, menu_x, menu_y, menu_w, menu_h, onebit::WHITE);
    onebit::drawRect(fb, menu_x, menu_y, menu_w, menu_h, onebit::BLACK);
    onebit::drawRect(fb, menu_x+2, menu_y+2, menu_w-4, menu_h-4, onebit::BLACK);

    int16_t text_x = menu_x + 6;
    int16_t text_y = menu_y + 5;
    for (int i = 0; i < ITEM_COUNT; ++i) {
        const char* label = itemLabel(i);
        if (i == selected_index_) {
            onebit::fillRect(fb, menu_x+3, text_y-1, menu_w-6, cell_h, onebit::BLACK);
            onebit::drawBitmapText(fb, font, text_x, text_y, label, onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, text_x, text_y, label, onebit::BLACK);
        }
        text_y += cell_h;
    }
}

} // namespace app
