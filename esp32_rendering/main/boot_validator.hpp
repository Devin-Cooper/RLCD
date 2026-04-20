#pragma once
#include "screen_stack.hpp"

namespace app {
// Spawn the boot-validator task. The task observes the render stack + heap +
// elapsed wall time and calls esp_ota_mark_app_valid_cancel_rollback() when
// the app proves it booted cleanly. The task deletes itself after marking
// (or after its 30s timeout).
//
// Production builds (CONFIG_TEST_CONSOLE_ENABLED=n): permissive stability
// check (DashboardScreen present anywhere in stack) + 5 s min elapsed + 30 s
// total timeout.
//
// Test builds (CONFIG_TEST_CONSOLE_ENABLED=y): skip the stability check.
// After 1 s elapsed, mark valid unconditionally — scenarios continuously
// inject disruptive events that look like rollback triggers to the
// permissive validator, so the test-build path runs through rollback
// testing via a dedicated slow-marked scenario instead.
void startBootValidatorTask(ScreenStack& stack);
} // namespace app
