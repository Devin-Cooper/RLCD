#pragma once
#include "screen.hpp"
#include "screen_context.hpp"

namespace app {

class ServerListScreen : public Screen {
public:
    explicit ServerListScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    int sel_ = 0;     // 0..count-1 = servers; count = "[+ Add new...]"
    int rowCount() const;
    void openEditorForSelection(ScreenStack& stack);
};

} // namespace app
