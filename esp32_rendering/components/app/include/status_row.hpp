#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>

namespace time_service { class TimeService; }

namespace app {

/// Top-of-screen 12 px status row. Currently renders only an HH:MM clock
/// (or "--:--" when TimeService reports time invalid). Drawn after the
/// stack render but before overlays in main's per-frame block, and only
/// when the top screen returns Screen::wantsStatusBar() == true.
class StatusRow {
public:
    explicit StatusRow(time_service::TimeService& ts);

    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);

    bool consumeDirty();

    static constexpr int kHeightPx = 12;

private:
    time_service::TimeService& ts_;
    int  lastMinute_ = -1;
    bool dirty_      = true;
    bool lastValid_  = false;
};

}  // namespace app
