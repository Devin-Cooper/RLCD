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

struct VectorFontSize {
    int16_t char_w, char_h, stroke, spacing;
};

VectorFontSize pickVectorSize(const char* text) {
    int len = static_cast<int>(std::strlen(text));
    if (len <= 2) return {90, 140, 6, 8};
    if (len == 3) return {70, 110, 5, 6};
    if (len == 4) return {55, 90,  4, 5};
    return                {40, 70,  3, 4};
}

void formatHeadline(const DashboardCard& card,
                    const DashboardSnapshot& snap,
                    char* out, size_t out_size) {
    auto fmt_pct = [&](float v) {
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        std::snprintf(out, out_size, "%.0f%%", v);
    };
    switch (card.source) {
        case MetricRef::Cpu: {
            float pct = snap.cpu_load[0] * 100.0f;
            fmt_pct(pct);
            break;
        }
        case MetricRef::Memory: fmt_pct(snap.mem_percent); break;
        case MetricRef::Disk:   fmt_pct(snap.disk_percent); break;
        case MetricRef::Uptime:
            std::snprintf(out, out_size, "%.6s",
                          snap.uptime_str[0] ? snap.uptime_str : "--");
            break;
        case MetricRef::Temp:
            {
                const char* s = snap.gpu_str;
                while (*s == '+' || *s == ' ') ++s;
                std::snprintf(out, out_size, "%.6s", s[0] ? s : "--");
            }
            break;
        case MetricRef::Screens:
            std::snprintf(out, out_size, "%.6s",
                          snap.screens_str[0] ? snap.screens_str : "--");
            break;
        case MetricRef::Custom: {
            int idx = card.command_index;
            const char* raw = (idx >= 0 && idx < snap.command_count &&
                               snap.command_outputs[idx]) ? snap.command_outputs[idx] : "--";
            std::snprintf(out, out_size, "%.6s", raw);
            break;
        }
        default:
            std::snprintf(out, out_size, "--");
            break;
    }
    for (char* p = out; *p; ++p) {
        if (!onebit::getGlyph(*p)) *p = '?';
    }
}

void renderHeadlineCardBody(onebit::IFramebuffer& fb,
                            const DashboardCard& card,
                            const DashboardSnapshot& snap) {
    char text[16];
    formatHeadline(card, snap, text, sizeof(text));
    if (!text[0] || std::strcmp(text, "--") == 0) {
        const onebit::BitmapFont& f = onebit::fonts::TERM_8X12;
        int w = onebit::getBitmapTextWidth(f, "--", 4);
        onebit::drawBitmapText(fb, f, (400 - w) / 2, 90, "--",
                               onebit::BLACK, 4);
        return;
    }

    VectorFontSize sz = pickVectorSize(text);
    int total_w = onebit::getStringWidth(text, sz.char_w, sz.spacing);
    int x = (400 - total_w) / 2;
    int y = 30 + (170 - sz.char_h) / 2;

    onebit::Pattern shadow_pat = onebit::benDay(128, 6, 2);
    onebit::drawVectorTextShadow(fb, x, y, text,
                                 sz.char_w, sz.char_h, sz.spacing, sz.stroke,
                                 /*dx=*/4, /*dy=*/5, shadow_pat);

    // Foreground knockout halo (wider WHITE stroke first), then BLACK fill.
    onebit::renderString(fb, text, x, y, sz.char_w, sz.char_h,
                         sz.spacing, sz.stroke + 4, onebit::WHITE);
    onebit::renderString(fb, text, x, y, sz.char_w, sz.char_h,
                         sz.spacing, sz.stroke, onebit::BLACK);
}

void renderHeadlineCard(onebit::IFramebuffer& fb,
                        const onebit::BitmapFont& font,
                        const DashboardCard& card,
                        const DashboardSnapshot& snap) {
    // Solid 2 px border
    onebit::drawRect(fb, 0, 0, 400, 300, onebit::BLACK);
    onebit::drawRect(fb, 1, 1, 398, 298, onebit::BLACK);

    renderHeadlineCardTitleStrip(fb, font, card, snap);

    onebit::fillPatternRect(fb, 2, 28, 396, 176, card.signature);

    renderHeadlineCardBody(fb, card, snap);
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
