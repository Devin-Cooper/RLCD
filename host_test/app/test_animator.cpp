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
