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

TEST_CASE("Animator: same tag overwrites in place", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::FocusRect, 0x01);
    a.start(tag, 0, 100, 120'000, 0);
    REQUIRE(a.value(tag, 60'000) > 0);
    a.start(tag, 200, 300, 120'000, 60'000);
    REQUIRE(a.value(tag, 60'000) == 200);
    REQUIRE(a.value(tag, 180'000) == 300);
}

TEST_CASE("Animator: snap-on-overflow drops silently", "[app][animator]") {
    Animator a;
    for (uint32_t i = 0; i < 8; ++i) {
        a.start(makeTag(TweenKind::FocusRect, i), 0, 100, 120'000, 0);
    }
    auto extra = makeTag(TweenKind::FocusRect, 100);
    a.start(extra, 0, 100, 120'000, 0);
    REQUIRE_FALSE(a.inProgress(extra, 60'000));
    REQUIRE(a.value(extra, 60'000) == 0);
}

TEST_CASE("Animator: tick prunes finished tweens", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::FocusRect, 0x01);
    a.start(tag, 0, 100, 120'000, 0);
    a.tick(200'000);
    auto tag2 = makeTag(TweenKind::FocusRect, 0x02);
    for (uint32_t i = 0; i < 7; ++i) {
        a.start(makeTag(TweenKind::ToastSlide, i), 0, 100, 120'000, 200'000);
    }
    a.start(tag2, 0, 50, 120'000, 200'000);
    REQUIRE(a.inProgress(tag2, 260'000));
}

TEST_CASE("Animator: cancel frees slot", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::ModalScale, 0x01);
    a.start(tag, 0, 100, 120'000, 0);
    a.cancel(tag);
    REQUIRE_FALSE(a.inProgress(tag, 60'000));
}

TEST_CASE("Animator: monotonic ease-out cubic", "[app][animator]") {
    Animator a;
    auto tag = makeTag(TweenKind::FocusRect, 0x01);
    a.start(tag, 0, 1000, 120'000, 0);
    int16_t prev = 0;
    for (int t = 0; t <= 120'000; t += 12'000) {
        int16_t cur = a.value(tag, t);
        REQUIRE(cur >= prev);
        prev = cur;
    }
}
