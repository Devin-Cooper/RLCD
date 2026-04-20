# RLCD Device Scenarios

On-device pytest scenarios driven over the ESP32-S3's built-in
USB-JTAG CDC via the `test_console` REPL. Complements the Catch2
host unit tests at `host_test/app/` — those are pure logic; these
run real firmware on connected hardware.

## Prerequisites

1. **Build and flash the firmware with `test_console` enabled**:
   ```bash
   cd esp32_rendering
   source ~/.espressif/v5.5.2/esp-idf/export.sh
   idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test" \
          -p /dev/cu.usbmodem4101 flash
   ```

2. **Install Python deps**:
   ```bash
   cd host_test/scenarios
   pip install -r requirements.txt
   ```

3. **Seed WiFi credentials** (one-time; persisted to NVS by the
   session fixture):
   ```bash
   export TEST_WIFI_SSID='your-ssid'
   export TEST_WIFI_PASS='your-pass'
   ```
   The scenarios assume WiFi is connected on boot — without it the
   firmware boots to WifiScreen instead of DashboardScreen and every
   navigation-based test fails.

## Run

```bash
cd host_test/scenarios
python -m pytest -v                         # all (~5 min)
python -m pytest -v -k "not slow"           # skip the 32s BLE timeout test
python -m pytest test_migration.py -v       # one file
```

## Environment variables

- `TEST_CONSOLE_PORT` — serial port (default `/dev/cu.usbmodem4101`)
- `TEST_CONSOLE_BAUD` — baud (default 460800; USB-JTAG CDC ignores it)
- `TEST_WIFI_SSID` / `TEST_WIFI_PASS` — seed creds for `fresh_device`
- `RLCD_ELF_PATH` — coredump ELF (default `../../esp32_rendering/build/esp32_terminal.elf`)

## Fixtures

- `device`: session-scoped, single open port. Seeds WiFi creds from
  env once at session start.
- `fresh_device`: reboots the device, erases `servers`/`app_settings`/
  `ssh_creds` NVS namespaces AND `/sdcard/servers/*.json` (preserves
  `wifi_creds` so the device reaches DashboardScreen, and preserves
  non-JSON files in `/sdcard/servers/` like SSH keys). Runs a warmup
  button press to drain the post-reboot first-event CDC race, then
  attaches a coredump-capture finalizer via `extract_coredump()`.
- `wifi_device`: `fresh_device` + one extra saved WiFi ("TestNetwork").
- `one_server_device`: `fresh_device` + one server at `127.0.0.1:22`.
  Loopback is used deliberately — any unreachable host hangs the
  `ssh_client` task past the task watchdog; 127.0.0.1 produces an
  immediate ECONNREFUSED via lwIP so state stays stable.

## Adding a new scenario

1. Write a pytest function in a `test_*.py` module using one of the fixtures.
2. Drive the device with `device.button(...)`, `device.key_*(...)`, etc.
3. Assert on `device.stack_top()`, `device.heap()`, `device.wifi_status()`, etc.
4. Use `device.expect_stack_top("<name-substring>", timeout=2.0)` for async
   stack transitions (e.g., after pushing a modal).

## Running a specific scenario

~~~bash
python -m pytest test_wifi.py::test_password_screen_type_submit_wrong_rejects -v
~~~

## Crash diagnosis

When a scenario fails with `DeviceCrashError`, the pytest output
includes a `coredump` section with the decoded backtrace
(via `esp-coredump info_corefile`). The corresponding base64
coredump blob is written to a tempfile whose path is printed in
the report for manual decoding if needed.

## Raising baud for large transfers

Default is 460800 (see `sdkconfig.test`). For `coredump-read` of a
1MB partition, 460800 takes ~22s; 921600 takes ~11s. Match the host
via `TEST_CONSOLE_BAUD=921600 python -m pytest ...`.

## Adding a command

1. Add the handler to the appropriate `commands_*.cpp` in the
   `test_console` component and register it.
2. Add a thin wrapper method on `Device` in `scenarios/device.py`.
3. Reference the wrapper from a scenario.
4. Run the full suite to make sure no existing scenario regresses.

## Adding a fixture

Fixtures are in `conftest.py`. Derive new ones from `fresh_device`
to inherit the crash-detection finalizer automatically.

## Concurrency caveats

The REPL task runs on Core 0 at FreeRTOS priority 5 and reads shared
state (`ScreenStack`, `ConfigManager`, `WifiManager`) that the main
render task mutates. The harness accepts a handful of benign races:

- **ScreenStack reads** (`depth`/`at`/`top`) are not internally locked.
  In practice `push`/`pop` only happen during `applyPending()` on the
  render task and the REPL only reads — a torn read of a
  `std::vector<unique_ptr<Screen>>::size()` is theoretically UB but
  produces at worst one stale snapshot. A `portMUX_TYPE` is the right
  fix but intentionally out of scope for the harness.

- **ConfigManager server table** is only written by the harness itself
  (via `server-upsert`/`server-delete`). Concurrent render-task reads
  during a write race only if `activeServer()` fires during the ~ms
  window of NVS persistence. Benign.

- **`s_testScanOverride`** (the fake-scan plumbing) is written by the
  REPL task and read by the render task when it handles a
  `WifiScanDone` event. The input-queue handoff provides happens-before.
  **Tests MUST call `system-wifi-scan-result` before
  `system-wifi-scan-inject`** — the render task won't peek at the
  override until the scan-done event fires.

If you add a new command that mutates shared state, audit against
these invariants: if the render task might read the same memory at
the same moment and the new commands aren't the sole writer, add a
mutex rather than accepting the race.
