#include "status_row.hpp"
#include "time_service.hpp"
#include <1bit/render/primitives.hpp>
#include <ctime>
#include <cstdio>
#include <cstring>

namespace app {

StatusRow::StatusRow(time_service::TimeService& ts) : ts_(ts) {}

void StatusRow::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    char text[8];
    bool valid = ts_.isTimeValid();

    if (!valid) {
        std::strcpy(text, "--:--");
    } else {
        time_t now = time(nullptr);
        struct tm tm_local{};
        localtime_r(&now, &tm_local);
        std::snprintf(text, sizeof(text), "%02d:%02d",
                      tm_local.tm_hour, tm_local.tm_min);
        if (tm_local.tm_min != lastMinute_) {
            dirty_ = true;
            lastMinute_ = tm_local.tm_min;
        }
    }
    if (valid != lastValid_) {
        dirty_ = true;
        lastValid_ = valid;
    }

    // Right-aligned with 4 px right margin. Use the actual measured width
    // (overlay.cpp uses onebit::getBitmapTextWidth — keep consistent).
    const int16_t text_w = onebit::getBitmapTextWidth(font, text);
    const int16_t x = static_cast<int16_t>(fb.width()) - text_w - 4;
    onebit::drawBitmapText(fb, font, x, /*y=*/2, text, onebit::BLACK);
}

bool StatusRow::consumeDirty() {
    bool d = dirty_;
    dirty_ = false;
    return d;
}

}  // namespace app
