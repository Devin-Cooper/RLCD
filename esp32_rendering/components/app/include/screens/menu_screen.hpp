#pragma once

#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

class MenuScreen : public Screen {
public:
    enum class Item : uint8_t {
        Dashboard = 0,
        Terminal,
        Servers,
        Settings,
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

    Item selected() const { return static_cast<Item>(selected_index_); }

private:
    ScreenContext& ctx_;
    uint8_t selected_index_ = 0;
    static constexpr int ITEM_COUNT = static_cast<int>(Item::Count);
    static const char* itemLabel(int index);
    void dispatchSelection(ScreenStack& stack, Item item);
};

} // namespace app
