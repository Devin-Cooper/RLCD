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
