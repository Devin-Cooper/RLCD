#include <catch2/catch_test_macros.hpp>
#include "overlay.hpp"

using app::OverlayManager;

TEST_CASE("Overlay: toast enqueue up to 3; 4th is dropped",
          "[app][overlay][toast]") {
    OverlayManager o;
    o.tick(0);
    REQUIRE(o.showToast("a", 1000));
    REQUIRE(o.showToast("b", 1000));
    REQUIRE(o.showToast("c", 1000));
    REQUIRE_FALSE(o.showToast("d", 1000));
    REQUIRE(o.activeToastCount() == 3);
    REQUIRE(o.droppedToastCount() == 1);
}

TEST_CASE("Overlay: toast dedup suppresses identical msg in 500ms",
          "[app][overlay][toast]") {
    OverlayManager o;
    o.tick(0);
    REQUIRE(o.showToast("same"));
    REQUIRE_FALSE(o.showToast("same"));
    REQUIRE(o.droppedToastCount() == 1);
    o.tick(600 * 1000);
    REQUIRE(o.showToast("same"));
}

TEST_CASE("Overlay: tick expires toasts", "[app][overlay][toast]") {
    OverlayManager o;
    o.tick(0);
    o.showToast("short", 100);
    REQUIRE(o.activeToastCount() == 1);
    o.tick(50 * 1000);
    REQUIRE(o.activeToastCount() == 1);
    o.tick(150 * 1000);
    REQUIRE(o.activeToastCount() == 0);
}
