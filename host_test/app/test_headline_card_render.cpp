#include <catch2/catch_test_macros.hpp>
#include "dashboard_cards.hpp"
#include <1bit/core/framebuffer.hpp>
#include <1bit/fonts/term_5x7.hpp>
#include <1bit/render/primitives.hpp>
#include <cstring>

using namespace app;
using namespace onebit;

namespace {
DashboardCard makeCpuCard() {
    DashboardCard c;
    c.layout = CardLayout::Headline;
    c.source = MetricRef::Cpu;
    c.command_index = 0;
    std::strncpy(c.label, "CPU", sizeof(c.label));
    c.signature = benDay(128, 6, 2);
    return c;
}

DashboardSnapshot makeBaseSnapshot() {
    DashboardSnapshot s{};
    s.cpu_load[0] = 0.47f;
    s.cpu_load[1] = 0.30f;
    s.cpu_load[2] = 0.25f;
    s.connected = true;
    s.last_update_ms = 100000;
    s.command_count = 1;
    static const char kOut[] = "0.47 0.30 0.25";
    s.command_outputs[0] = kOut;
    return s;
}
} // namespace

TEST_CASE("Headline border + title strip drawn", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    auto card = makeCpuCard();
    auto snap = makeBaseSnapshot();
    renderCard(fb, fonts::TERM_5X7, card, snap);

    // Border: top-left corner pixel should be black (2 px border).
    CHECK(fb.getPixel(0, 0) == BLACK);
    CHECK(fb.getPixel(399, 0) == BLACK);
    CHECK(fb.getPixel(0, 299) == BLACK);
    CHECK(fb.getPixel(399, 299) == BLACK);
    CHECK(fb.getPixel(1, 1) == BLACK);

    // Inside the frame at (4, 26) — below the title strip (y=2..23) — must
    // be inside the pattern band region. Pattern fill is non-empty.
    bool any_band_black = false;
    for (int16_t y = 28; y < 50 && !any_band_black; ++y) {
        for (int16_t x = 4; x < 396 && !any_band_black; ++x) {
            if (fb.getPixel(x, y) == BLACK) any_band_black = true;
        }
    }
    CHECK(any_band_black);

    // Title region (y=2..23) has at least some BLACK pixels (LineScreen fill).
    bool any_title_black = false;
    for (int16_t y = 2; y < 24 && !any_title_black; ++y) {
        for (int16_t x = 2; x < 398 && !any_title_black; ++x) {
            if (fb.getPixel(x, y) == BLACK) any_title_black = true;
        }
    }
    CHECK(any_title_black);
}

TEST_CASE("Headline numeral drawn in band region (vector font)", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    auto card = makeCpuCard();
    auto snap = makeBaseSnapshot();
    renderCard(fb, fonts::TERM_5X7, card, snap);

    // Vector-font "47%" lives roughly inside (50,30)..(350,200). Count BLACK
    // pixels in the headline area; expect a healthy chunk (more than the
    // pattern band alone would produce).
    int headline = 0;
    for (int16_t y = 30; y < 200; ++y) {
        for (int16_t x = 50; x < 350; ++x) {
            if (fb.getPixel(x, y) == BLACK) headline++;
        }
    }
    INFO("headline BLACK pixel count: " << headline);
    // Empirical: BenDay-128 pattern band alone produces 12600 BLACK pixels in
    // (50..350,30..200). With "47%" headline (white knockout halo cuts band,
    // BLACK foreground + drop shadow add ink) the count rises to ~14268.
    // Threshold of 13500 cleanly separates "headline drawn" from
    // "band-only fill" while leaving headroom for glyph-shape variation.
    CHECK(headline > 13500);
}

TEST_CASE("Headline drop-shadow paints below+right of the foreground", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    auto card = makeCpuCard();
    auto snap = makeBaseSnapshot();
    renderCard(fb, fonts::TERM_5X7, card, snap);

    int left_count = 0;
    int right_count = 0;
    for (int16_t y = 100; y < 130; ++y) {
        for (int16_t x = 50; x < 200; ++x) if (fb.getPixel(x, y) == BLACK) left_count++;
        for (int16_t x = 200; x < 350; ++x) if (fb.getPixel(x, y) == BLACK) right_count++;
    }
    INFO("y=100..130 left: " << left_count << " right: " << right_count);
    // Empirical: band-only baseline in this strip is 1125 pixels per side
    // (BenDay-128 is uniform). With foreground glyphs + shadow the counts
    // rise to ~1448 left / ~1331 right — both strictly above 1200.
    CHECK(left_count > 1200);
    CHECK(right_count > 1200);
}

TEST_CASE("Headline L2 row contains 'Load 0.47'", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    auto card = makeCpuCard();
    auto snap = makeBaseSnapshot();
    renderCard(fb, fonts::TERM_5X7, card, snap);

    int l2_black = 0;
    for (int16_t y = 208; y < 220; ++y) {
        for (int16_t x = 8; x < 392; ++x) {
            if (fb.getPixel(x, y) == BLACK) l2_black++;
        }
    }
    CHECK(l2_black > 30);
}

TEST_CASE("Headline L3 row 1 shows raw command output", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    auto card = makeCpuCard();
    auto snap = makeBaseSnapshot();
    renderCard(fb, fonts::TERM_5X7, card, snap);

    int l3_black = 0;
    for (int16_t y = 224; y < 232; ++y) {
        for (int16_t x = 8; x < 392; ++x) {
            if (fb.getPixel(x, y) == BLACK) l3_black++;
        }
    }
    CHECK(l3_black > 20);
}

TEST_CASE("renderPipStrip: 7 cards → 7 dots, active is filled", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    renderPipStrip(fb, /*current=*/2, /*prev=*/2, /*card_count=*/7, /*underline_x=*/0);

    int center_x_active = 200 - (7 * 12) / 2 + 2 * 12 + 6;  // approx
    int black_at_active = 0;
    for (int16_t y = 286; y < 290; ++y) {
        for (int16_t x = center_x_active - 4; x < center_x_active + 4; ++x) {
            if (fb.getPixel(x, y) == BLACK) black_at_active++;
        }
    }
    CHECK(black_at_active >= 5);
}

TEST_CASE("renderPipStrip with card_count<=1 is a no-op", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);
    renderPipStrip(fb, 0, 0, 1, 0);
    for (int16_t y = 282; y < 296; ++y) {
        for (int16_t x = 2; x < 398; ++x) {
            CHECK(fb.getPixel(x, y) == WHITE);
        }
    }
}

TEST_CASE("Disconnected: pattern band uses sparse BlueNoise + L3 says 'no connection'",
          "[app][dashboard][render][disconnected]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    auto card = makeCpuCard();
    auto snap = makeBaseSnapshot();
    snap.connected = false;

    renderCard(fb, fonts::TERM_5X7, card, snap);

    int dim_band_black = 0;
    for (int16_t y = 28; y < 204; ++y) {
        for (int16_t x = 2; x < 398; ++x) {
            if (fb.getPixel(x, y) == BLACK) dim_band_black++;
        }
    }
    CHECK(dim_band_black > 0);
    CHECK(dim_band_black < 396 * 176);

    int l3b_black = 0;
    for (int16_t y = 240; y < 248; ++y) {
        for (int16_t x = 8; x < 392; ++x) {
            if (fb.getPixel(x, y) == BLACK) l3b_black++;
        }
    }
    CHECK(l3b_black > 0);
}
