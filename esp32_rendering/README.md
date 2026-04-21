# ESP32 Terminal (`esp32_rendering` subproject)

ESP-IDF firmware for the Waveshare ESP32-S3-RLCD-4.2 board. Turns the board into a self-contained SSH terminal and server dashboard driven by a Bluetooth LE keyboard.

> Directory is named `esp32_rendering/` for historical reasons. The CMake
> `project(esp32_terminal)` call determines the artifact name, so the
> compiled binary is `build/esp32_terminal.bin`.

## What it does

- **Terminal mode** — VT100/xterm emulator rendering vim, htop, and other TUI apps on the 400x300 1-bit ST7305 display. Three font sizes (5x7 / 6x9 / 8x12).
- **Dashboard mode** — curated server stats (CPU, memory, disk, GPU, Docker, screen sessions) refreshed on an interval; commands are user-defined.
- **SSH client** — libssh 0.11.4 (vendored in-tree as `components/libssh/`) with hardware-accelerated AES-128-CTR, Ed25519 / ECDSA P-256 / RSA-SHA2 auth, TOFU host key verification.
- **BLE HID host** — NimBLE central-role HID keyboard host with bonded-device auto-reconnect and full keycode translation (arrows, F-keys, Ctrl combos).
- **WiFi** — STA auto-connect to known networks ranked by signal strength.
- **SD-card multi-server config** — drop JSON files into `/sdcard/servers/` to register servers; the menu lets you switch between them live.
- **Physical buttons + menu overlay** — works without a keyboard for basic navigation.

## Project layout

```
esp32_rendering/
├── CMakeLists.txt              # Top-level ESP-IDF project (project(esp32_terminal))
├── sdkconfig.defaults          # Canonical Kconfig defaults (sdkconfig regenerated each build)
├── partitions.csv              # 16MB flash layout: OTA x2, LittleFS, coredump
├── idf.sh                      # Wrapper that locates ESP-IDF 5.5.x and runs idf.py
├── main/
│   ├── main.cpp                # Boot sequence, mode switching, input dispatch
│   └── idf_component.yml       # Pulls joltwallet/littlefs
├── components/
│   ├── app/                    # Menu, dashboard, terminal mode, settings
│   ├── ble_hid/                # NimBLE HID keyboard host
│   ├── buttons/                # Debounced button handler
│   ├── i2c_bsp/                # I2C bus abstraction
│   ├── input_queue/            # Unified FreeRTOS event queue
│   ├── rendering/              # Legacy rendering primitives (superseded by onebit)
│   ├── sdcard_config/          # SD card FAT mount + JSON server config parser
│   ├── sensors/                # RTC (PCF85063), SHTC3, battery
│   ├── ssh_client/             # libssh wrapper + TOFU host key store
│   ├── st7305/                 # ST7305 1-bit SPI display driver
│   └── wifi_manager/           # WiFi STA lifecycle + NVS credential storage
```

## Prerequisites

- **ESP-IDF v5.5.x** (required; the `idf.sh` wrapper enforces the version).
- **[1bit-display](https://github.com/tinkeringtanuki/1bit-display) library** checked out somewhere on disk.
- Waveshare ESP32-S3-RLCD-4.2 board.

## Building

The top-level `CMakeLists.txt` looks for `1bit-display` via the `ONEBIT_LIB_DIR` env var, falling back to `../../1bit-display` (sibling to the RLCD repo).

```bash
cd esp32_rendering

# Option A: rely on the default location (../../1bit-display)
./idf.sh build

# Option B: point to a custom location
ONEBIT_LIB_DIR=/absolute/path/to/1bit-display ./idf.sh build

# Or with vanilla ESP-IDF
source ~/.espressif/v5.5.2/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

Flash and monitor:

```bash
./idf.sh flash monitor
# or
idf.py -p /dev/ttyUSB0 flash monitor
```

### First flash after a `partitions.csv` change

If you ever see `E (...) esp_image: image at 0x...: has invalid magic` on boot, the on-device partition table is out of sync. Erase the whole chip:

```bash
./idf.sh erase-flash
./idf.sh flash
```

## Crash diagnostics

Coredumps are enabled (ELF format, CRC32-checked, up to 64 tasks) and written to the dedicated `coredump` partition. After a crash:

```bash
./idf.sh coredump-info         # summary
./idf.sh coredump-debug        # gdb with full symbols
```

## First-time setup

1. Flash firmware and power on the board.
2. Long-press Button A to enter BLE pairing mode (30 s window).
3. Pair a BLE keyboard from its own "add device" flow.
4. Via the on-screen menu or keyboard shortcut, open Servers and pick an SD-card-defined server, or edit credentials from the menu.
5. Authenticate — password on first connect. To use key auth, copy an Ed25519 PEM private key into LittleFS and set `key_path` in the server JSON to its absolute path (upload via a later tooling story).

### Server config file format

Place one JSON file per server in `/sdcard/servers/`:

```json
{
  "name": "home-lab",
  "host": "192.168.1.10",
  "port": 22,
  "username": "pi",
  "dashboard": [
    {"label": "CPU", "cmd": "cat /proc/loadavg"},
    {"label": "Memory", "cmd": "free -m"},
    {"label": "Disk", "cmd": "df -h"}
  ]
}
```

Files larger than 8 KiB are skipped (see `components/sdcard_config/src/config_manager.cpp`).

## ST7305 display notes

The ST7305 reflective LCD requires Display Inversion Mode ON (0x21) to prevent flicker with high-frequency dither patterns. The driver inverts pixel logic (framebuffer initialised to 0xFF, BLACK clears the corresponding bit).

## Pin configuration

| Signal   | GPIO | Notes |
|----------|------|-------|
| SPI MOSI | 12   | LCD data |
| SPI SCK  | 11   | LCD clock |
| LCD DC   | 5    | Data/Command |
| LCD CS   | 40   | Chip select |
| LCD RST  | 41   | Reset |
| I2C SDA  | 6    | RTC, temp/humidity |
| I2C SCL  | 7    | RTC, temp/humidity |
| BAT_ADC  | 4    | ADC1_CH3, 3:1 divider |

## License

MIT — see [../LICENSE](../LICENSE).
