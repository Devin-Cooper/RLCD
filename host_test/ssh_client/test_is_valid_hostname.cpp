#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>

// Paste-for-paste copy of the production helper (ssh_client.cpp). Keep in
// lockstep with the production definition — any behavioural drift here is
// a bug.
namespace {
static bool is_valid_hostname(const char* s) {
    if (!s) return false;
    size_t n = 0;
    while (s[n] != '\0') ++n;
    if (n == 0 || n > 253) return false;
    if (s[0] == '-' || s[0] == '.') return false;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        bool ok =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}
} // namespace

TEST_CASE("is_valid_hostname accepts well-formed hosts", "[ssh][sanitize]") {
    REQUIRE(is_valid_hostname("example.com"));
    REQUIRE(is_valid_hostname("srv01.local"));
    REQUIRE(is_valid_hostname("192.168.1.10"));
    REQUIRE(is_valid_hostname("homelab_beta-2"));
    REQUIRE(is_valid_hostname("a"));
    REQUIRE(is_valid_hostname("A.B.C.D"));
}

TEST_CASE("is_valid_hostname rejects path traversal", "[ssh][sanitize]") {
    REQUIRE_FALSE(is_valid_hostname("../etc/passwd"));
    REQUIRE_FALSE(is_valid_hostname("../../secrets"));
    REQUIRE_FALSE(is_valid_hostname("foo/bar"));
    REQUIRE_FALSE(is_valid_hostname("/etc/passwd"));
    REQUIRE_FALSE(is_valid_hostname(".hidden"));
    REQUIRE_FALSE(is_valid_hostname("-flag"));
}

TEST_CASE("is_valid_hostname rejects empty and null", "[ssh][sanitize]") {
    REQUIRE_FALSE(is_valid_hostname(""));
    REQUIRE_FALSE(is_valid_hostname(nullptr));
}

TEST_CASE("is_valid_hostname rejects overlong and non-ASCII", "[ssh][sanitize]") {
    char long_host[300];
    std::memset(long_host, 'a', 254);
    long_host[254] = '\0';
    REQUIRE_FALSE(is_valid_hostname(long_host));

    REQUIRE_FALSE(is_valid_hostname("hello world"));
    REQUIRE_FALSE(is_valid_hostname("injection;rm"));
    REQUIRE_FALSE(is_valid_hostname("tab\there"));
    REQUIRE_FALSE(is_valid_hostname("n\xC3\xA9l"));
}

TEST_CASE("is_valid_hostname accepts 253-char boundary", "[ssh][sanitize]") {
    char host[254];
    std::memset(host, 'a', 253);
    host[253] = '\0';
    REQUIRE(is_valid_hostname(host));
}
