#include "dashboard_cards.hpp"
#include <1bit/render/primitives.hpp>

namespace app {

void renderCard(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                const DashboardCard& card, const DashboardSnapshot& snapshot) {
    (void)card; (void)snapshot;
    // Filled in by Tasks 9–13.
    onebit::drawBitmapText(fb, font, 10, 10, "renderCard stub", onebit::BLACK);
}

void renderPipStrip(onebit::IFramebuffer& fb, int current_card, int prev_card,
                    int card_count, int16_t underline_x) {
    (void)fb; (void)current_card; (void)prev_card; (void)card_count; (void)underline_x;
    // Filled in by Task 12.
}

void renderEmptyState(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    const char* msg = "No dashboard commands";
    int w = onebit::getBitmapTextWidth(font, msg, 1);
    onebit::drawBitmapText(fb, font, (400 - w) / 2, 140, msg, onebit::BLACK, 1);
}

} // namespace app
