#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "animator.hpp"
#include "command_ids.hpp"
#include "wifi_manager.hpp"
#include <algorithm>

namespace app {

class WifiScreen : public Screen {
public:
    enum class Tab : uint8_t { Known, Available };

    explicit WifiScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

private:
    ScreenContext& ctx_;
    Tab tab_ = Tab::Available;
    int sel_ = 0;
    wifi::NetworkInfo scan_[wifi::WifiManager::MAX_SCAN_RESULTS];
    int scan_count_ = 0;
    bool scan_in_flight_ = false;

    wifi::NetworkInfo known_[wifi::WifiManager::MAX_KNOWN_NETWORKS];
    int known_count_ = 0;
    void refreshKnown();

    void startScan();
    void refreshScanResults();
    void onEnterTab(Tab t);

    static void sanitize(char* dst, const char* src, size_t dst_cap);

    int visibleCount() const;
    void connectSelected(ScreenStack& stack);

    // Phase 5: focus-rect animation
    int16_t prev_selected_y_ = 0;
    bool    focus_y_initialized_ = false;
    int16_t list_start_y_ = 0;
    int16_t row_h_ = 0;
    int16_t computeRowY(int index) const;
    void    onSelectionChange(int old_index, int new_index);
};

} // namespace app
