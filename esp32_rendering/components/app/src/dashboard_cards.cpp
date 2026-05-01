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

void renderHeadlineCardFooter(onebit::IFramebuffer& fb,
                              const onebit::BitmapFont& font_5x7,
                              const DashboardCard& card,
                              const DashboardSnapshot& snap) {
    const onebit::BitmapFont& f8 = onebit::fonts::TERM_8X12;

    char l2[64] = {};
    switch (card.source) {
        case MetricRef::Cpu:
            std::snprintf(l2, sizeof(l2), "Load %.2f  5m %.2f  15m %.2f",
                          snap.cpu_load[0], snap.cpu_load[1], snap.cpu_load[2]);
            break;
        case MetricRef::Memory:
            std::snprintf(l2, sizeof(l2), "%.1f / %.0f GB",
                          snap.mem_used_gb, snap.mem_total_gb);
            break;
        case MetricRef::Disk:
            std::snprintf(l2, sizeof(l2), "%s / %s",
                          snap.disk_used_str[0] ? snap.disk_used_str : "?",
                          snap.disk_total_str[0] ? snap.disk_total_str : "?");
            break;
        case MetricRef::Uptime:
            std::snprintf(l2, sizeof(l2), "%s",
                          snap.uptime_str[0] ? snap.uptime_str : "(no data)");
            break;
        case MetricRef::Temp:
            std::snprintf(l2, sizeof(l2), "%s",
                          snap.gpu_str[0] ? snap.gpu_str : "(no data)");
            break;
        case MetricRef::Screens:
            std::snprintf(l2, sizeof(l2), "screen sessions");
            break;
        default:
            std::snprintf(l2, sizeof(l2), "%s", card.label);
            break;
    }
    onebit::drawBitmapText(fb, f8, 8, 208, l2, onebit::BLACK, 1);

    int idx = card.command_index;
    const char* raw = (idx >= 0 && idx < snap.command_count &&
                       snap.command_outputs[idx]) ? snap.command_outputs[idx] : "";
    char l3a[96] = {};
    int wp = 0;
    for (int i = 0; raw[i] && wp < (int)sizeof(l3a) - 1 && wp < 80; ++i) {
        char c = raw[i];
        if (c == '\n' || c == '\r') c = ' ';
        l3a[wp++] = c;
    }
    onebit::drawBitmapText(fb, font_5x7, 8, 224, l3a, onebit::BLACK, 1);

    char l3b[48] = {};
    if (!snap.connected) {
        std::snprintf(l3b, sizeof(l3b), "no connection");
    } else if (snap.last_update_ms > 0) {
        std::snprintf(l3b, sizeof(l3b), "updated recently");
    } else {
        std::snprintf(l3b, sizeof(l3b), "updated --");
    }
    onebit::drawBitmapText(fb, font_5x7, 8, 240, l3b, onebit::BLACK, 1);
}

void renderHeadlineCard(onebit::IFramebuffer& fb,
                        const onebit::BitmapFont& font,
                        const DashboardCard& card,
                        const DashboardSnapshot& snap) {
    // Solid 2 px border
    onebit::drawRect(fb, 0, 0, 400, 300, onebit::BLACK);
    onebit::drawRect(fb, 1, 1, 398, 298, onebit::BLACK);

    renderHeadlineCardTitleStrip(fb, font, card, snap);

    onebit::Pattern band_pat = snap.connected ? card.signature : onebit::blueNoise(32);
    onebit::fillPatternRect(fb, 2, 28, 396, 176, band_pat);

    renderHeadlineCardBody(fb, card, snap);
    renderHeadlineCardFooter(fb, font, card, snap);
}

void drawSparklineWithPattern(onebit::IFramebuffer& fb,
                              int x, int y, int w, int h,
                              const float* data, int data_len, int head,
                              float max_val, const onebit::Pattern& fill_pattern) {
    if (max_val <= 0) max_val = 1;
    if (w <= 0 || h <= 0 || data_len <= 0) return;
    int prev_vy = -1;
    for (int px = 0; px < w; px++) {
        int sample_idx = (px * data_len) / w;
        int idx = (head - data_len + sample_idx + data_len) % data_len;
        float val = data[idx];
        if (val > max_val) val = max_val;
        int vy = y + h - 1 - static_cast<int>((val / max_val) * (h - 1));
        if (vy < y + h) {
            onebit::fillPatternRect(fb, x + px, vy, 1, y + h - vy, fill_pattern);
        }
        fb.setPixel(x + px, vy, onebit::BLACK);
        if (prev_vy >= 0) {
            onebit::drawLine(fb, x + px - 1, prev_vy, x + px, vy, onebit::BLACK);
        }
        prev_vy = vy;
    }
}

void renderTrendsCard(onebit::IFramebuffer& fb,
                      const onebit::BitmapFont& font,
                      const DashboardCard& card,
                      const DashboardSnapshot& snap) {
    onebit::drawRect(fb, 0, 0, 400, 300, onebit::BLACK);
    onebit::drawRect(fb, 1, 1, 398, 298, onebit::BLACK);
    renderHeadlineCardTitleStrip(fb, font, card, snap);

    char now_buf[24];
    std::snprintf(now_buf, sizeof(now_buf), "now %.2f", snap.cpu_load[0]);
    int now_w = onebit::getBitmapTextWidth(onebit::fonts::TERM_8X12, now_buf, 1);
    onebit::drawBitmapText(fb, onebit::fonts::TERM_8X12, 8, 30, "CPU 60s", onebit::BLACK, 1);
    onebit::drawBitmapText(fb, onebit::fonts::TERM_8X12, 392 - now_w, 30, now_buf, onebit::BLACK, 1);

    float cpu_max = 1.0f;
    for (int i = 0; i < 60; ++i) if (snap.cpu_history[i] > cpu_max) cpu_max = snap.cpu_history[i];
    cpu_max *= 1.2f;
    drawSparklineWithPattern(fb, 8, 44, 384, 92,
                             snap.cpu_history, 60, snap.history_pos,
                             cpu_max, onebit::benDay(128, 6, 2));

    std::snprintf(now_buf, sizeof(now_buf), "now %.0f%%", snap.mem_percent);
    now_w = onebit::getBitmapTextWidth(onebit::fonts::TERM_8X12, now_buf, 1);
    onebit::drawBitmapText(fb, onebit::fonts::TERM_8X12, 8, 144, "Memory 60s", onebit::BLACK, 1);
    onebit::drawBitmapText(fb, onebit::fonts::TERM_8X12, 392 - now_w, 144, now_buf, onebit::BLACK, 1);

    drawSparklineWithPattern(fb, 8, 158, 384, 84,
                             snap.mem_history, 60, snap.history_pos,
                             100.0f, onebit::halftone(128, 8));
}

} // namespace

void renderCard(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                const DashboardCard& card, const DashboardSnapshot& snap) {
    switch (card.layout) {
        case CardLayout::Headline: renderHeadlineCard(fb, font, card, snap); break;
        case CardLayout::Trends:   renderTrendsCard(fb, font, card, snap); break;
    }
}

namespace {
constexpr int16_t kPipStripY      = 282;
constexpr int16_t kPipStripH      = 14;
constexpr int16_t kPipDotRadius   = 4;
constexpr int16_t kPipDotSpacing  = 12;

int16_t pipUnderlineXForFn(int card_idx, int card_count) {
    int16_t total_w = static_cast<int16_t>(card_count * kPipDotSpacing);
    int16_t left = static_cast<int16_t>((400 - total_w) / 2);
    return static_cast<int16_t>(left + card_idx * kPipDotSpacing + kPipDotSpacing / 2);
}
} // namespace

int16_t pipUnderlineXFor(int card_idx, int card_count) {
    return pipUnderlineXForFn(card_idx, card_count);
}

int wrapCardIndex(int current, int delta, int card_count) {
    if (card_count <= 0) return 0;
    return ((current + delta) % card_count + card_count) % card_count;
}

void renderPipStrip(onebit::IFramebuffer& fb, int current_card, int prev_card,
                    int card_count, int16_t underline_x) {
    if (card_count <= 1) return;

    int16_t cy = kPipStripY + kPipStripH / 2 - 2;
    for (int i = 0; i < card_count; ++i) {
        int16_t cx = pipUnderlineXForFn(i, card_count);
        if (i == current_card) {
            onebit::fillCircle(fb, cx, cy, kPipDotRadius, onebit::BLACK);
        } else {
            onebit::drawCircle(fb, cx, cy, kPipDotRadius, onebit::BLACK);
        }
    }
    if (underline_x <= 0) underline_x = pipUnderlineXForFn(current_card, card_count);
    onebit::fillRect(fb, underline_x - kPipDotRadius, kPipStripY + kPipStripH - 3,
                     kPipDotRadius * 2, 2, onebit::BLACK);

    (void)prev_card;
}

void renderEmptyState(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);
    const char* msg = "No dashboard commands";
    int w = onebit::getBitmapTextWidth(font, msg, 1);
    onebit::drawBitmapText(fb, font, (400 - w) / 2, 140, msg, onebit::BLACK, 1);
}

} // namespace app
