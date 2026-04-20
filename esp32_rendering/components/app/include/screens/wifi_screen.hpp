#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "wifi_manager.hpp"

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

private:
    ScreenContext& ctx_;
    Tab tab_ = Tab::Available;
    int sel_ = 0;
    wifi::NetworkInfo scan_[wifi::WifiManager::MAX_SCAN_RESULTS];
    int scan_count_ = 0;
    bool scan_in_flight_ = false;

    void startScan();
    void refreshScanResults();
    void onEnterTab(Tab t);

    static void sanitize(char* dst, const char* src, size_t dst_cap);

    int visibleCount() const;
    void connectSelected(ScreenStack& stack);
};

} // namespace app
