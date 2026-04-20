# RLCD Device Scenarios

On-device pytest scenarios driven over UART via the `test_console` REPL.
Complements the Catch2 host unit tests at `host_test/app/` — those are
pure logic; these run real firmware on connected hardware.

## Prerequisites

1. **Build the firmware with `test_console` enabled**:
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

## Run

```bash
cd host_test/scenarios
python -m pytest -v
```

## Environment variables

- `TEST_CONSOLE_PORT` — serial port (default `/dev/cu.usbmodem4101`)
- `TEST_CONSOLE_BAUD` — baud (default 460800; must match Kconfig)
- `RLCD_ELF_PATH` — coredump ELF path (default `../../esp32_rendering/build/rlcd.elf`)

## Fixtures

- `device`: session-scoped, single open port
- `fresh_device`: reboot + erase all app NVS namespaces; auto-captures coredump on crash
- `wifi_device`: `fresh_device` + one saved WiFi ("TestNetwork")
- `one_server_device`: `fresh_device` + one configured server ("test")

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
