// host_test/app/test_ssh_keys_id.cpp
#include <catch2/catch_test_macros.hpp>
#include "ssh_key_types.hpp"

#include <set>
#include <string>

using namespace ssh_keys;

TEST_CASE("hex roundtrip", "[ssh_keys_id]") {
    for (int i = 0; i < 100; ++i) {
        auto id = KeyId::random();
        auto hex = id.hex();
        REQUIRE(hex.size() == 32);
        auto back = KeyId::parse(hex);
        REQUIRE(back.has_value());
        REQUIRE(*back == id);
    }
}

TEST_CASE("parse rejects bad input", "[ssh_keys_id]") {
    REQUIRE_FALSE(KeyId::parse("").has_value());
    REQUIRE_FALSE(KeyId::parse("0123456789abcdef0123456789abcdeG").has_value());  // bad hex
    REQUIRE_FALSE(KeyId::parse("00112233445566778899aabbccddeeff00").has_value());  // too long
    REQUIRE_FALSE(KeyId::parse("00112233445566778899aabbccddeef").has_value());  // too short
}

TEST_CASE("random uniqueness at 10k", "[ssh_keys_id]") {
    std::set<std::string> seen;
    for (int i = 0; i < 10000; ++i) {
        auto id = KeyId::random();
        REQUIRE(seen.insert(id.hex()).second);
    }
}
