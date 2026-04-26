#include <catch2/catch_test_macros.hpp>
#include "path_helpers.hpp"

using fb::path::join;
using fb::path::parent;
using fb::path::basename;
using fb::path::truncateBreadcrumb;

TEST_CASE("path::join", "[path]") {
    REQUIRE(join("/sdcard", "foo")          == "/sdcard/foo");
    REQUIRE(join("/sdcard/", "foo")         == "/sdcard/foo");
    REQUIRE(join("/sdcard", "/foo")         == "/sdcard/foo");
    REQUIRE(join("/sdcard/", "/foo")        == "/sdcard/foo");
    REQUIRE(join("",        "foo")          == "foo");
    REQUIRE(join("/",       "foo")          == "/foo");
    REQUIRE(join("/sdcard", "")             == "/sdcard");
}

TEST_CASE("path::parent", "[path]") {
    REQUIRE(parent("/sdcard/foo/bar")  == "/sdcard/foo");
    REQUIRE(parent("/sdcard/foo")      == "/sdcard");
    REQUIRE(parent("/sdcard")          == "/");
    REQUIRE(parent("/")                == "/");
    REQUIRE(parent("")                 == "/");
    REQUIRE(parent("/sdcard/foo/")     == "/sdcard");
}

TEST_CASE("path::basename", "[path]") {
    REQUIRE(basename("/sdcard/foo/bar.txt") == "bar.txt");
    REQUIRE(basename("/sdcard")             == "sdcard");
    REQUIRE(basename("/")                   == "");
    REQUIRE(basename("foo")                 == "foo");
    REQUIRE(basename("/sdcard/foo/")        == "foo");  // trailing slash stripped
}

TEST_CASE("path::truncateBreadcrumb", "[path]") {
    // Short path → unchanged.
    REQUIRE(truncateBreadcrumb("/sdcard/foo", 80) == "/sdcard/foo");
    // Truncated middle: keeps leading "/sdcard/" prefix and trailing tail.
    auto t = truncateBreadcrumb("/sdcard/aaaaaaaaaaaaaaaa/bbbbbbbb/ccc.txt", 24);
    REQUIRE(t.size() <= 24);
    REQUIRE(t.find("...") != std::string::npos);
    REQUIRE(t.substr(t.size() - 7) == "ccc.txt");
}
