# host_test — Host-Side Unit Tests

Standalone C++17 test harness for RLCD pure-logic code. Runs on macOS
and Linux via Catch2 v3.x. **No ESP-IDF required.**

## Quickstart

```bash
cd host_test
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass. `ctest` prints one line per TEST_CASE.

## Directory layout

```
host_test/
├── CMakeLists.txt          # Top-level; FetchContent Catch2 v3.5.2; add_subdirectory(...)
├── cmake/AddHostTest.cmake # add_host_test() helper
├── shims/                  # ESP-IDF header shims
│   ├── esp_log.h           # ESP_LOGx → fprintf(stderr, ...)
│   ├── esp_err.h           # esp_err_t = int + constants
│   ├── esp_timer.h         # stub types + no-op functions
│   └── freertos/           # FreeRTOS.h, task.h, queue.h (real std::deque + mutex)
├── smoke/                  # Framework self-tests
├── ble_hid/                # translate_keycode, process_hid_report
├── app/                    # dashboard_feed, menu_navigation
│   └── stubs/1bit/         # IFramebuffer/BitmapFont/primitives no-op stubs
└── buttons/                # Button state machine (readGpioLevel seam)
    └── driver/gpio.h       # Host stub for ESP-IDF driver/gpio.h
```

## Shim policy

**Production source compiled for host tests must be ESP-IDF-free except
via shims.** The shim headers under `host_test/shims/` satisfy ESP-IDF
includes (`esp_log.h`, `esp_err.h`, `esp_timer.h`, `freertos/*.h`). If
a test needs a new ESP-IDF header, add a minimal shim — keep the
library small.

**Do NOT** include NimBLE, LittleFS, LVGL, NVS, or the `onebit` rendering
library from host-tested code. If a production file depends on one of
these, either:
1. Extract the pure logic into a separate ESP-IDF-free TU (preferred),
   as done for `ble_hid/hid_translate.{cpp,hpp}`,
   `ble_hid/hid_report_process.{cpp,hpp}`, and
   `app/dashboard_feed.{cpp,hpp}`.
2. Provide a stub header under `host_test/<component>/stubs/` that
   compiles but does nothing, as done for
   `host_test/app/stubs/1bit/...`.

## Adding a new test

1. Pick the component: `host_test/<component>/test_<target>.cpp`.
2. Write test cases using Catch2 macros (`TEST_CASE`, `SECTION`, `REQUIRE`).
3. Edit `host_test/<component>/CMakeLists.txt` and add an `add_host_test(...)` block.
4. If your component directory isn't yet referenced from the top-level
   `CMakeLists.txt`, add `add_subdirectory(<component>)` there.
5. Rebuild and run.

## Conventions

- No glob — list test sources explicitly for CMake cache correctness.
- Tests must be hermetic: no network, filesystem, env vars, or clock deps
  (`esp_timer_get_time` returns 0 via the shim).
- Test names use `test_<snake_case>` pattern matching the target.
- Tags in `TEST_CASE` use lowercase bracketed scopes: `"[<component>][<feature>]"`.

## Running a single test

```bash
./build/ble_hid/test_translate_keycode "[translate]"
./build/ble_hid/test_translate_keycode "translateKeycode: arrow keys"
```

## CI

`.github/workflows/host-tests.yml` runs this sequence on `ubuntu-latest`
and `macos-latest` for every push.
