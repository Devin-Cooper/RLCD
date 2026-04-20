#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "dashboard.hpp"
#include "ssh_client.hpp"

namespace app {

/// Thin Screen adapter over the existing app::Dashboard class.
/// Delegates render/update to Dashboard; Btn A short opens the menu
/// (via ctx_.openLegacyMenu during the 2a/2b transition; replaced by
/// MenuScreen push when AppMode is deleted in Task 14).
class DashboardScreen : public Screen {
public:
    explicit DashboardScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    int targetFps() const override { return 10; }

    void tickUpdate(int64_t now_ms);
    void feedSshData(const uint8_t* data, size_t len);

private:
    ScreenContext& ctx_;
};

} // namespace app
