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
