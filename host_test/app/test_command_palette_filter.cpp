#include <catch2/catch_test_macros.hpp>
#include "command_palette_filter.hpp"

using app::icontains;

TEST_CASE("icontains: empty needle matches everything", "[app][palette]") {
    REQUIRE(icontains("anything", ""));
    REQUIRE(icontains("", ""));
}

TEST_CASE("icontains: case-insensitive substring", "[app][palette]") {
    REQUIRE(icontains("Connect to: Server-1", "server"));
    REQUIRE(icontains("Connect to: Server-1", "SERVER"));
    REQUIRE(icontains("Connect to: Server-1", "server-1"));
    REQUIRE_FALSE(icontains("Open dashboard", "wifi"));
    REQUIRE_FALSE(icontains("ABC", "ABCD"));
}

TEST_CASE("icontains: null haystack with non-empty needle", "[app][palette]") {
    REQUIRE_FALSE(icontains(nullptr, "x"));
}
