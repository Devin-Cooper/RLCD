#include <catch2/catch_test_macros.hpp>
#include "dashboard_cards.hpp"
#include <1bit/core/framebuffer.hpp>
#include <1bit/fonts/term_5x7.hpp>
#include <cstring>

using namespace app;
using namespace onebit;

TEST_CASE("Trends card: title strip + two sparkline rows + pip strip", "[app][dashboard][render]") {
    DynamicFramebuffer fb(400, 300);
    fb.clear(WHITE);

    DashboardCard card;
    card.layout = CardLayout::Trends;
    card.source = MetricRef::None;
    card.command_index = -1;
    std::strncpy(card.label, "Trends", sizeof(card.label));
    card.signature = lineScreen(128, 35, 2);

    DashboardSnapshot snap{};
    snap.connected = true;
    snap.last_update_ms = 100000;
    for (int i = 0; i < 60; ++i) {
        snap.cpu_history[i] = 0.5f * i / 60;
        snap.mem_history[i] = 30.0f + 30.0f * i / 60;
    }
    snap.history_pos = 60;
    snap.cpu_load[0] = 0.42f;
    snap.mem_percent = 47.0f;

    renderCard(fb, fonts::TERM_5X7, card, snap);

    int title_black = 0;
    for (int16_t y = 2; y < 24; ++y) {
        for (int16_t x = 2; x < 398; ++x) {
            if (fb.getPixel(x, y) == BLACK) title_black++;
        }
    }
    CHECK(title_black > 100);

    int cpu_black = 0;
    for (int16_t y = 44; y < 136; ++y) {
        for (int16_t x = 8; x < 392; ++x) {
            if (fb.getPixel(x, y) == BLACK) cpu_black++;
        }
    }
    CHECK(cpu_black > 30);

    int mem_black = 0;
    for (int16_t y = 158; y < 242; ++y) {
        for (int16_t x = 8; x < 392; ++x) {
            if (fb.getPixel(x, y) == BLACK) mem_black++;
        }
    }
    CHECK(mem_black > 30);
}
