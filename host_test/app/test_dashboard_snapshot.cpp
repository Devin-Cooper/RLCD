// Verifies the data-model split: Dashboard exposes parsed values via
// snapshot() instead of private members. We can't easily host-test the
// SSH/parse flow (Dashboard depends on ssh_client, esp_log, etc.), so
// this test exercises only the *shape* of DashboardSnapshot and the
// public accessors. The on-device scenario test_dashboard_disconnected
// exercises end-to-end parsing.

#include <catch2/catch_test_macros.hpp>

// Forward-declare just the snapshot struct shape for the test —
// the real definition lives in dashboard.hpp but pulling in the
// full header drags ssh/log/idf bits we don't have on host. We
// satisfy the test by `static_assert`'ing fields exist on a tiny
// stub via a wrapper header.
#include "dashboard_snapshot.hpp"

using app::DashboardSnapshot;

TEST_CASE("DashboardSnapshot has the documented fields", "[app][dashboard]") {
    DashboardSnapshot s{};
    s.cpu_load[0] = 1.0f;
    s.mem_percent = 50.0f;
    s.disk_percent = 30.0f;
    s.history_pos = 5;
    s.cpu_history[0] = 0.5f;
    s.mem_history[0] = 25.0f;
    s.command_count = 3;
    s.command_outputs[0] = "hello";
    s.connected = true;
    s.last_update_ms = 1234567;
    s.interval_ms = 5000;

    REQUIRE(s.cpu_load[0] == 1.0f);
    REQUIRE(s.command_outputs[0] != nullptr);
    REQUIRE(s.connected);
}
