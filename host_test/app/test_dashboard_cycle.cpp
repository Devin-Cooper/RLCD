#include <catch2/catch_test_macros.hpp>
#include "dashboard_cards.hpp"
#include "animator.hpp"

using namespace app;

TEST_CASE("wrapCardIndex wraps forward and backward", "[app][dashboard][cycle]") {
    REQUIRE(wrapCardIndex(0, 1, 5) == 1);
    REQUIRE(wrapCardIndex(4, 1, 5) == 0);
    REQUIRE(wrapCardIndex(0, -1, 5) == 4);
    REQUIRE(wrapCardIndex(2, -3, 5) == 4);
    REQUIRE(wrapCardIndex(0, 0, 1) == 0);
    REQUIRE(wrapCardIndex(0, 1, 0) == 0);
}

TEST_CASE("Pip animation: tween from prev to cur fires", "[app][dashboard][cycle]") {
    Animator a;
    constexpr uint32_t tag = makeTag(TweenKind::DashboardPip, 0);

    int16_t from_x = pipUnderlineXFor(0, 5);
    int16_t to_x   = pipUnderlineXFor(1, 5);
    a.start(tag, from_x, to_x, kDashboardPipUs, 0);

    REQUIRE(a.value(tag, 0) == from_x);
    REQUIRE(a.value(tag, kDashboardPipUs) == to_x);
    REQUIRE(a.value(tag, kDashboardPipUs / 2) > from_x);
    REQUIRE(a.value(tag, kDashboardPipUs / 2) < to_x);
}
