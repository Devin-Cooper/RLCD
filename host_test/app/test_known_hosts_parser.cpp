#include <catch2/catch_test_macros.hpp>
#include "known_hosts_parser.hpp"

using namespace ssh;

TEST_CASE("blank and comment lines return nullopt", "[known_hosts]") {
    REQUIRE_FALSE(parse_known_hosts_entry("").has_value());
    REQUIRE_FALSE(parse_known_hosts_entry("   \t  ").has_value());
    REQUIRE_FALSE(parse_known_hosts_entry("# this is a comment").has_value());
    REQUIRE_FALSE(parse_known_hosts_entry("   # leading whitespace then comment").has_value());
}

TEST_CASE("plain-host entry parses", "[known_hosts]") {
    auto e = parse_known_hosts_entry("example.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI");
    REQUIRE(e.has_value());
    REQUIRE(e->host == "example.com");
    REQUIRE(e->key_type == "ssh-ed25519");
    REQUIRE(e->blob_b64 == "AAAAC3NzaC1lZDI1NTE5AAAAI");
    REQUIRE(e->comment.empty());
    REQUIRE_FALSE(e->is_hashed);
}

TEST_CASE("bracketed host:port parses", "[known_hosts]") {
    auto e = parse_known_hosts_entry("[127.0.0.1]:2222 ssh-ed25519 AAAAblob");
    REQUIRE(e.has_value());
    REQUIRE(e->host == "[127.0.0.1]:2222");
    REQUIRE_FALSE(e->is_hashed);
}

TEST_CASE("hashed host recognised", "[known_hosts]") {
    auto e = parse_known_hosts_entry("|1|abcHashBase64==|ownerHashBase64== ssh-ed25519 AAAAblob");
    REQUIRE(e.has_value());
    REQUIRE(e->is_hashed);
    REQUIRE(e->host == "|1|abcHashBase64==|ownerHashBase64==");
}

TEST_CASE("comment field preserved verbatim", "[known_hosts]") {
    auto e = parse_known_hosts_entry("example.com ssh-ed25519 AAAAblob my dev server");
    REQUIRE(e.has_value());
    REQUIRE(e->comment == "my dev server");
}

TEST_CASE("too-few-fields rejected", "[known_hosts]") {
    REQUIRE_FALSE(parse_known_hosts_entry("host.com ssh-ed25519").has_value());
    REQUIRE_FALSE(parse_known_hosts_entry("host.com").has_value());
}

TEST_CASE("whole-file parser skips blanks and comments", "[known_hosts]") {
    std::string_view content =
        "# Header\n"
        "example.com ssh-ed25519 AAAAone\n"
        "\n"
        "[1.2.3.4]:2222 ecdsa-sha2-nistp256 AAAAtwo my-server\n"
        "|1|hashedhost ssh-rsa AAAAthree\n";
    auto entries = parse_known_hosts_file(content);
    REQUIRE(entries.size() == 3);
    REQUIRE(entries[0].host == "example.com");
    REQUIRE(entries[1].comment == "my-server");
    REQUIRE(entries[2].is_hashed);
}
