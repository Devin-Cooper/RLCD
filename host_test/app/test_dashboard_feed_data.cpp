#include <catch2/catch_test_macros.hpp>
#include "dashboard_feed.hpp"

#include <array>
#include <cstring>
#include <string>

using app::FeedBuffer;
using app::FeedStatus;
using app::feedChunk;

namespace {
constexpr int CAP = 256;  // matches Dashboard::MAX_OUTPUT_LEN today

struct Fixture {
    std::array<char, CAP> buf{};
    FeedBuffer fb{buf.data(), 0, CAP, false};

    void feed_str(const std::string& s) {
        feedChunk(fb, reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }
};
} // namespace

TEST_CASE("feedChunk: sentinel in one shot yields Complete and trims tail", "[app][dashboard]") {
    Fixture f;
    std::string s = "output\n__DASH_END__\n";
    FeedStatus st = feedChunk(f.fb,
                              reinterpret_cast<const uint8_t*>(s.data()), s.size());
    REQUIRE(st == FeedStatus::Complete);
    REQUIRE(std::string(f.fb.output, f.fb.output_len) == "output");
}

TEST_CASE("feedChunk: sentinel split byte-by-byte completes exactly once", "[app][dashboard]") {
    Fixture f;
    // Stop feeding once Complete fires — subsequent bytes go to the next command.
    const std::string stream = "x\n__DASH_END__";
    int completes = 0;
    for (char c : stream) {
        uint8_t b = static_cast<uint8_t>(c);
        if (feedChunk(f.fb, &b, 1) == FeedStatus::Complete) ++completes;
    }
    REQUIRE(completes == 1);
    REQUIRE(std::string(f.fb.output, f.fb.output_len) == "x");
}

TEST_CASE("feedChunk: skip_echo drops until first newline", "[app][dashboard]") {
    Fixture f;
    f.fb.skip_echo = true;
    f.feed_str("echoed command line\nreal output\n__DASH_END__\n");
    REQUIRE(std::string(f.fb.output, f.fb.output_len) == "real output");
    REQUIRE(f.fb.skip_echo == false);
}

TEST_CASE("feedChunk: bracketed-paste escapes are stripped on completion", "[app][dashboard]") {
    Fixture f;
    f.feed_str(std::string("\x1b[?2004l", 8) + "hello" +
               std::string("\x1b[?2004h", 8) + "__DASH_END__\n");
    REQUIRE(std::string(f.fb.output, f.fb.output_len) == "hello");
}

TEST_CASE("feedChunk: sentinel-straddle across every chunk boundary", "[app][dashboard]") {
    // Stop at the sentinel's last char — trailing bytes belong to the next cmd.
    const std::string s = "abc\n__DASH_END__";
    for (std::size_t k = 1; k < s.size(); ++k) {
        Fixture f;
        auto first = reinterpret_cast<const uint8_t*>(s.data());
        FeedStatus a = feedChunk(f.fb, first, k);
        FeedStatus b = feedChunk(f.fb, first + k, s.size() - k);
        INFO("split at k=" << k);
        REQUIRE((a == FeedStatus::Complete || b == FeedStatus::Complete));
        REQUIRE_FALSE((a == FeedStatus::Complete && b == FeedStatus::Complete));
        REQUIRE(std::string(f.fb.output, f.fb.output_len) == "abc");
    }
}

// CHARACTERIZATION: documents pre-Spec-04 truncation-hang behavior.
// When a single command's output fills the buffer to capacity-1, appending
// stops but the sentinel scan keeps running against the saturated buffer
// and can never find __DASH_END__. feedChunk returns Continue forever.
// Spec 04 replaces this with an overflow flag + timeout fallback; when
// that lands, this test flips to assert the new behavior.
TEST_CASE("feedChunk: 2000-byte stream without sentinel clamps and never completes (pre-Spec-04)",
          "[app][dashboard][pre-spec-04]") {
    Fixture f;
    std::string big(2000, 'x');
    FeedStatus st = feedChunk(f.fb,
        reinterpret_cast<const uint8_t*>(big.data()), big.size());
    REQUIRE(st == FeedStatus::Continue);
    REQUIRE(f.fb.output_len == CAP - 1);

    std::string sent = "\n__DASH_END__\n";
    FeedStatus st2 = feedChunk(f.fb,
        reinterpret_cast<const uint8_t*>(sent.data()), sent.size());
    REQUIRE(st2 == FeedStatus::Continue);
}
