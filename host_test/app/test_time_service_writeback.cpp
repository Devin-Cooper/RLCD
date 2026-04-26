#include <catch2/catch_test_macros.hpp>
#include "time_service.hpp"

using time_service::detail::sanityGuardEpoch;
using time_service::detail::utcEpochToRtcTime;

TEST_CASE("sanityGuardEpoch: rejects pre-2023 timestamps",
          "[time_service][writeback]") {
    REQUIRE_FALSE(sanityGuardEpoch(0));
    REQUIRE_FALSE(sanityGuardEpoch(1));
    REQUIRE_FALSE(sanityGuardEpoch(1699999999));
}

TEST_CASE("sanityGuardEpoch: accepts 2023-11-14T22:13:20Z and later",
          "[time_service][writeback]") {
    REQUIRE(sanityGuardEpoch(1700000000));   // 2023-11-14T22:13:20Z
    REQUIRE(sanityGuardEpoch(2000000000));   // 2033-05-18
    REQUIRE(sanityGuardEpoch(4000000000));   // far future
}

TEST_CASE("utcEpochToRtcTime: round-trips a known UTC instant",
          "[time_service][writeback]") {
    // 2026-04-26T12:30:45Z -> epoch 1777206645 (Sunday, weekday=0)
    time_t ts = 1777206645;
    sensors::RtcTime out{};
    utcEpochToRtcTime(ts, out);
    REQUIRE(out.year   == 2026);
    REQUIRE(out.month  == 4);
    REQUIRE(out.day    == 26);
    REQUIRE(out.hour   == 12);
    REQUIRE(out.minute == 30);
    REQUIRE(out.second == 45);
    REQUIRE(out.weekday == 0);  // Sunday
}
