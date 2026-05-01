#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "dashboard.hpp"
#include "dashboard_cards.hpp"
#include "ssh_client.hpp"
#include "animator.hpp"
#include <array>

namespace app {

class DashboardScreen : public Screen {
public:
    explicit DashboardScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    int targetFps() const override { return 10; }
    bool bypassesKeyboardGate() const override { return true; }
    ScreenKind screenKind() const override { return ScreenKind::Dashboard; }
    const char* breadcrumbLabel() const override { return "Dashboard"; }
    bool wantsKeybindFooter() const override { return false; }

    void tickUpdate(int64_t now_ms);
    void feedSshData(const uint8_t* data, size_t len);

    // Test introspection — used by the dashboard-active-card REPL command.
    int currentCardIndex() const { return current_card_; }
    int cardCount() const { return card_count_; }
    const DashboardCard& cardAt(int i) const { return cards_[i]; }

private:
    static constexpr uint32_t kPipTag =
        makeTag(TweenKind::DashboardPip, 0);
    static constexpr int kCardArraySize = 9;  // 8 commands + Trends

    void rebuildCards();
    void advance(int delta, int64_t now_ms);
    void jumpTo(int idx, int64_t now_ms);

    ScreenContext& ctx_;
    std::array<DashboardCard, kCardArraySize> cards_{};
    int     card_count_       = 0;
    int     current_card_     = 0;
    int     prev_card_        = 0;
    int64_t card_started_ms_  = 0;
};

} // namespace app
