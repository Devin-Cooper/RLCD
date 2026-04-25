#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "animator.hpp"

using app::Animator;
using app::easeOutCubic;
using app::TweenKind;
using app::makeTag;
using Catch::Approx;

TEST_CASE("easeOutCubic: boundaries", "[app][animator]") {
    REQUIRE(easeOutCubic(0.0f) == Approx(0.0f));
    REQUIRE(easeOutCubic(1.0f) == Approx(1.0f));
    REQUIRE(easeOutCubic(0.5f) == Approx(0.875f).margin(1e-5f));
}

TEST_CASE("Animator: value at t=0 returns from", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::FocusRect, 0x01);
    a.start(tag, 10, 100, 120'000, 0);
    REQUIRE(a.value(tag, 0) == 10);
}

TEST_CASE("Animator: value at t>=duration returns to", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::FocusRect, 0x01);
    a.start(tag, 10, 100, 120'000, 0);
    REQUIRE(a.value(tag, 120'000) == 100);
    REQUIRE(a.value(tag, 200'000) == 100);
}

TEST_CASE("Animator: inProgress flips at duration", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::FocusRect, 0x01);
    a.start(tag, 0, 100, 120'000, 0);
    REQUIRE(a.inProgress(tag, 60'000));
    REQUIRE_FALSE(a.inProgress(tag, 120'000));
    REQUIRE_FALSE(a.inProgress(tag, 200'000));
}
