# esp32_recovery

Recovery firmware for the ESP32-S3-RLCD-4.2 board — a sibling ESP-IDF
project that builds a minimal image living in the `factory` partition
(512 KB). The bootloader auto-boots recovery when the main application
fails to call `esp_ota_mark_app_valid_cancel_rollback()` within its
30 s window; recovery keeps USB-JTAG CDC visible so the host can
always reflash main without touching the BOOT button.

See [../MIGRATION-factory-ota.md](../MIGRATION-factory-ota.md) for the
partition layout and first-time flash procedure.

## What recovery does

1. Init PSRAM + ST7305 panel (shared driver with the main app).
2. Render a single-shot "RECOVERY MODE" banner + build info + reset
   reason on the 400×300 reflective LCD.
3. Spawn a 1 Hz heartbeat task that toggles a bottom-right pixel so
   frozen vs running is visually distinguishable.
4. Start an `esp_console` REPL on the USB-JTAG CDC endpoint.

## REPL commands

| Command | Behavior |
|---|---|
| `ping` | `>>> OK pong uptime=<us>` |
| `info` | Version / build date / SHA / running partition / otadata state / reset reason / free heap. Prints `state=n/a` when running from factory (factory has no otadata tracking). |
| `reboot` | `esp_restart()` |
| `reboot-ota` | `esp_ota_set_boot_partition(ota_0)` + `esp_restart()`. Errors if no ota_0 partition exists. |
| `erase-nvs --yes` | `nvs_flash_erase()` then `nvs_flash_init()`. Requires the `--yes` arg so accidental paste doesn't wipe WiFi creds. Leaves `otadata` alone — a subsequent `reboot-ota` still works. |
| `coredump-dump` | Streams the coredump partition in base64 DATA chunks. Same format as the main app's `coredump-read`, so `esp-coredump info_corefile` can decode the output. |

Response markers:

```
>>> OK [payload]
>>> ERR <code> <msg>
>>> DATA <line>
```

No mutex on the response emitters — only the REPL task writes. This
differs from `test_console` where ESP_LOGx output is interleaved.

## Building standalone

```bash
source ~/.espressif/v5.5.2/esp-idf/export.sh
idf.py -C esp32_recovery build
idf.py -C esp32_recovery -p /dev/cu.usbmodem4101 flash
```

Standalone flash writes to 0x20000 (factory slot), the matching
bootloader, and the partition table. Main app in ota_0 is untouched.

## Not included (deliberate)

- **No WiFi, Bluetooth, lwIP, mbedTLS.** `sdkconfig.defaults` explicitly
  sets `CONFIG_ESP_WIFI_ENABLED=n` and `CONFIG_BT_ENABLED=n`.
- **No OTA-over-UART / network push.** Recovery's role is to keep the
  USB-JTAG CDC endpoint stable so `esptool` from the host can always
  flash ota_0. Network OTA is out of scope for v1.
- **No SD card, no I²C sensors, no buttons.** Recovery renders once and
  idles; no user input besides the REPL.
- **No shared `display_bsp` component.** Panel bring-up is copied from
  `esp32_rendering/main/main.cpp` into `recovery_screen.cpp`. Extracting
  a common `display_bsp` is a future refactor, deliberately out of
  scope for this project.

## Self-update

**Recovery does not self-update.** The `factory` partition is treated
as write-once-from-host — reflashed only via `./flash.sh` at major
release cuts. Espressif's own guidance is to avoid writing the factory
partition from a running app (power-loss mid-write bricks the device
to the ROM download state, which then requires a BOOT+RESET + esptool
recovery).

## Size budget

Current binary: ~237 KB (≈45% of the 512 KB slot). Ample headroom for
future display tweaks or additional REPL commands. CI enforces a
< 512 KB limit in `scripts/verify_builds.sh`.

The single biggest consumer is the ST7305 driver + rendering primitives
(~60 KB combined); after that `esp_console` (~30 KB) and `nvs_flash`
(~30 KB). Disable any of these only if you know what you're giving up.
