#include <catch2/catch_test_macros.hpp>
#include "time_service.hpp"
#include <ctime>
#include <cstdlib>

using time_service::detail::localTimeToUtcEpoch;

TEST_CASE("localTimeToUtcEpoch: UTC0 passes time through unchanged",
          "[time_service][tz]") {
    sensors::RtcTime local{2026, 4, 26, 14, 30, 0, 0};
    time_t ts;
    REQUIRE(localTimeToUtcEpoch(local, "UTC0", ts));
    struct tm tm{};
    gmtime_r(&ts, &tm);
    REQUIRE(tm.tm_year == 126);
    REQUIRE(tm.tm_mon  == 3);
    REQUIRE(tm.tm_mday == 26);
    REQUIRE(tm.tm_hour == 14);
    REQUIRE(tm.tm_min  == 30);
}

TEST_CASE("localTimeToUtcEpoch: JST-9 subtracts 9h to reach UTC",
          "[time_service][tz]") {
    sensors::RtcTime local{2026, 4, 26, 14, 30, 0, 0};
    time_t ts;
    REQUIRE(localTimeToUtcEpoch(local, "JST-9", ts));
    struct tm tm{};
    gmtime_r(&ts, &tm);
    REQUIRE(tm.tm_hour == 5);
    REQUIRE(tm.tm_min  == 30);
}

TEST_CASE("localTimeToUtcEpoch: PST/PDT in summer => UTC-7",
          "[time_service][tz]") {
    // 2026-07-15 14:30 PDT (UTC-7) -> 21:30 UTC
    sensors::RtcTime local{2026, 7, 15, 14, 30, 0, 0};
    time_t ts;
    REQUIRE(localTimeToUtcEpoch(local, "PST8PDT,M3.2.0,M11.1.0", ts));
    struct tm tm{};
    gmtime_r(&ts, &tm);
    REQUIRE(tm.tm_hour == 21);
    REQUIRE(tm.tm_min  == 30);
}

TEST_CASE("localTimeToUtcEpoch: PST/PDT in winter => UTC-8",
          "[time_service][tz]") {
    // 2026-01-15 14:30 PST (UTC-8) -> 22:30 UTC
    sensors::RtcTime local{2026, 1, 15, 14, 30, 0, 0};
    time_t ts;
    REQUIRE(localTimeToUtcEpoch(local, "PST8PDT,M3.2.0,M11.1.0", ts));
    struct tm tm{};
    gmtime_r(&ts, &tm);
    REQUIRE(tm.tm_hour == 22);
    REQUIRE(tm.tm_min  == 30);
}

TEST_CASE("localTimeToUtcEpoch: rejects out-of-range fields",
          "[time_service][tz]") {
    time_t ts;
    REQUIRE_FALSE(localTimeToUtcEpoch({2023, 4, 26, 12, 0, 0, 0}, "UTC0", ts));   // year < 2024
    REQUIRE_FALSE(localTimeToUtcEpoch({2100, 4, 26, 12, 0, 0, 0}, "UTC0", ts));   // year > 2099
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 0, 26, 12, 0, 0, 0}, "UTC0", ts));   // month 0
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 13, 26, 12, 0, 0, 0}, "UTC0", ts));  // month 13
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 4, 0, 12, 0, 0, 0}, "UTC0", ts));    // day 0
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 4, 32, 12, 0, 0, 0}, "UTC0", ts));   // day 32
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 4, 26, 24, 0, 0, 0}, "UTC0", ts));   // hour 24
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 4, 26, 12, 60, 0, 0}, "UTC0", ts));  // minute 60
    REQUIRE_FALSE(localTimeToUtcEpoch({2026, 4, 26, 12, 0, 60, 0}, "UTC0", ts));  // second 60
}

TEST_CASE("Catalog entries all parse as valid POSIX TZ strings",
          "[time_service][tz][catalog]") {
    size_t count = 0;
    auto* cat = time_service::TimeService::catalog(&count);
    REQUIRE(count > 0);
    for (size_t i = 0; i < count; ++i) {
        sensors::RtcTime local{2026, 6, 15, 12, 0, 0, 0};
        time_t ts;
        INFO("catalog[" << i << "] label=" << cat[i].label
             << " posix_tz=" << cat[i].posix_tz);
        REQUIRE(localTimeToUtcEpoch(local, cat[i].posix_tz, ts));
    }
}
