#include "dashboard_cards.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/vector_font.hpp>
#include <1bit/fonts/term_5x7.hpp>
#include <1bit/fonts/term_8x12.hpp>
#include <cstdio>
#include <cstring>

namespace app {

namespace {

void renderHeadlineCardTitleStrip(onebit::IFramebuffer& fb,
                                  const onebit::BitmapFont& font,
                                  const DashboardCard& card,
                                  const DashboardSnapshot& snap) {
    // LineScreen-35 deg fill behind the title text
    onebit::Pattern title_fill = onebit::lineScreen(128, 35, 2);
    onebit::fillPatternRect(fb, 2, 2, 396, 22, title_fill);

    // Centered card label in 8x12 with a 4 px white knockout
    const onebit::BitmapFont& title_font = onebit::fonts::TERM_8X12;
    int label_w = onebit::getBitmapTextWidth(title_font, card.label, 1);
    int label_x = (400 - label_w) / 2;
    onebit::fillRect(fb, label_x - 4, 2, label_w + 8, 22, onebit::WHITE);
    onebit::drawBitmapText(fb, title_font, label_x, 7,
                           card.label, onebit::BLACK, 1);

    // Right-aligned "Upd HH:MM:SS" in 5x7
    char ts_buf[24];
    if (snap.last_update_ms > 0) {
        int total_s = static_cast<int>(snap.last_update_ms / 1000) % 86400;
        int hh = total_s / 3600;
        int mm = (total_s % 3600) / 60;
        int ss = total_s % 60;
        std::snprintf(ts_buf, sizeof(ts_buf), "Upd %02d:%02d:%02d", hh, mm, ss);
    } else {
        std::snprintf(ts_buf, sizeof(ts_buf), "Upd --:--:--");
    }
    int ts_w = onebit::getBitmapTextWidth(font, ts_buf, 1);
    onebit::fillRect(fb, 400 - ts_w - 6, 2, ts_w + 4, 22, onebit::WHITE);
    onebit::drawBitmapText(fb, font, 400 - ts_w - 4, 10, ts_buf,
                           onebit::BLACK, 1);
}

void renderHeadlineCard(onebit::IFramebuffer& fb,
                        const onebit::BitmapFont& font,
                        const DashboardCard& card,
                        const DashboardSnapshot& snap) {
    // Solid 2 px border
    onebit::drawRect(fb, 0, 0, 400, 300, onebit::BLACK);
    onebit::drawRect(fb, 1, 1, 398, 298, onebit::BLACK);

    renderHeadlineCardTitleStrip(fb, font, card, snap);

    // Pattern band — Tasks 10–11 will fill in with the headline + L2/L3.
    onebit::fillPatternRect(fb, 2, 28, 396, 176, card.signature);
}

} // namespace

void renderCard(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                const DashboardCard& card, const DashboardSnapshot& snap) {
    switch (card.layout) {
        case CardLayout::Headline: renderHeadlineCard(fb, font, card, snap); break;
        case CardLayout::Trends:   /* Task 13 */ break;
    }
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
