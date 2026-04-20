# test_console

On-device REPL for automated scenario testing. Production builds exclude
this component entirely — it is gated by `CONFIG_TEST_CONSOLE_ENABLED`
(default `n`). When disabled the component registers zero source files
and contributes no code.

## Enabling

Use the `sdkconfig.test` overlay:

```bash
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test" flash
```

The overlay also routes the primary console to USB-JTAG CDC (the
ESP32-S3-RLCD board has no external UART bridge) and enables
FreeRTOS trace for the `tasklist` command.

## Protocol

Commands are read line-by-line from the USB-JTAG CDC endpoint. Every
handler emits response markers on stdout:

```
>>> OK [payload]     # success, optional trailing payload
>>> ERR <code> <msg> # failure with integer code
>>> DATA <line>      # one line of multi-line payload
```

Response writes are guarded by a FreeRTOS mutex so interleaved
ESP_LOGx output can't tear a marker line.

## Command groups

- `commands_runtime.cpp` — ping, uptime, log level, reboot, crash,
  coredump check/read/erase,
  ota-info → running partition metadata + otadata state + app version/sha.
- `commands_introspect.cpp` — stack (screen-stack walk via RTTI),
  heap, tasklist, wifi/ssh/ble status, migration result, fb dump
  (PGM P5 framebuffer over base64).
- `commands_injection.cpp` — btn A/B short/long, key-press, key-enter,
  key-esc, key-tab, key-backspace, key-arrow, key-fn, key-ctrl,
  key-raw, system-wifi-state, system-ble-state, system-wifi-scan-*.
- `commands_nvs.cpp` — nvs-get/set/rm/erase, wifi-save/forget/known,
  server-upsert/delete/set-active/list, settings-set.
- `commands_fs.cpp` — fs-ls, fs-read, fs-rm, fs-mkdir.
- `commands_fs_write.cpp` — tokened chunked upload
  (fs-write-begin/chunk/commit/abort).

## Host side

See `host_test/scenarios/README.md` for the Python `Device` class
that wraps the protocol + the pytest fixtures that drive scenarios.

## Concurrency caveats

The REPL task runs on Core 0 at FreeRTOS priority 5 and reads
shared state (ScreenStack, ConfigManager, WifiManager) that the
main render task mutates. The harness accepts a handful of benign
races — see the scenarios README's "Concurrency caveats" section
for the full analysis.
