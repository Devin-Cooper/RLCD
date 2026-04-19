#include <catch2/catch_test_macros.hpp>

TEST_CASE("smoke: arithmetic sanity", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
