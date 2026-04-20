# RLCD

Playground for building and testing different setups for the Waveshare ESP32-S3-RLCD-4.2 development board.

## Board Overview

The ESP32-S3-RLCD-4.2 is a development board featuring a 4.2" reflective LCD that requires no backlight.

| Component | Specification |
|-----------|---------------|
| **Display** | 4.2" RLCD, 400×300 pixels, 1-bit monochrome, SPI (ST7305 driver) |
| **SoC** | ESP32-S3-WROOM-1-N16R8 (dual-core Xtensa LX7 @ 240MHz) |
| **Memory** | 16MB Flash, 8MB PSRAM, 512KB SRAM |
| **Connectivity** | WiFi 2.4GHz, Bluetooth 5 LE |
| **Audio** | ES8311 codec, ES7210 ADC with dual-mic array |
| **Sensors** | SHTC3 (temperature/humidity) |
| **RTC** | PCF85063 |
| **Power** | 18650 battery holder, USB-C |
| **Storage** | TF card slot (FAT32) |

## Projects

### esp32_terminal (SSH Terminal & Dashboard)

ESP-IDF firmware that turns the board into a self-contained SSH terminal and server dashboard. Connect a Bluetooth keyboard, point it at a Linux server, and get a full interactive shell on the 400x300 reflective display.

#### Features

- **Terminal mode** -- full VT100/xterm emulator rendering htop, vim, and other TUI apps on the 1-bit display
- **Dashboard mode** -- curated server stats (CPU, memory, disk, network, GPU, Docker, screen sessions) refreshed on a configurable interval
- **SSH client** -- libssh2 with hardware-accelerated AES-128-CTR (7.5 MB/s), Ed25519 auth (26ms), TOFU host key verification
- **BLE keyboard** -- NimBLE HID host with auto-reconnect to bonded devices, full keycode-to-terminal translation (arrows, function keys, Ctrl combos)
- **WiFi** -- auto-connect to known networks by signal strength; new networks are added via SD card JSON config (on-screen password entry is not implemented).
- **3 font sizes** -- 80x37 (5x7), 66x30 (6x9), 50x23 (8x12) -- cycle with button or F-key
- **Menu system** -- overlay navigable via keyboard or physical buttons
- **Power management** -- WiFi modem-sleep during active SSH (~20mA). Light sleep is disabled because ST7305 needs continuous SPI power.
- **OTA updates** -- dual 3MB app partitions with automatic rollback

#### Hardware-Informed Optimizations

The firmware is tuned for the ESP32-S3's specific hardware profile:

| Decision | Why |
|----------|-----|
| AES-128-CTR over AES-GCM | Hardware AES-CTR: 7.5 MB/s. AES-GCM GHASH in software: 1.35 MB/s (5.5x slower) |
| Ed25519 over RSA-2048 | 26ms sign vs 118ms. Curve25519 software math is faster than RSA hardware bignum |
| Framebuffer in DMA SRAM | SPI DMA requires internal SRAM. PSRAM would need cache coherence workaround |
| Scrollback in PSRAM | Large, sequential access pattern benefits from 64-byte cache line prefetch |
| 64KB data cache | Trades 32KB SRAM for substantially better PSRAM throughput |
| NimBLE over Bluedroid | Saves ~170KB flash and ~30KB RAM. ESP32-S3 is BLE-only anyway |
| SSH on Core 1 | Isolates crypto from WiFi/BLE protocol stacks on Core 0 |

#### Architecture

```
Core 0 (Protocol)           Core 1 (Application)
WiFi Driver (P:23)          Display/SPI DMA (P:12)
BLE Controller (P:23)       SSH/libssh2 (P:10)
lwIP TCP/IP (P:18)          ANSI Parser (P:8)
BLE HID Input (P:9)         Dashboard Poll (P:5)
```

#### Flash Partition Layout (16MB)

| Partition | Size | Purpose |
|-----------|------|---------|
| NVS | 24 KB | WiFi/SSH credentials (plaintext; encryption deferred) |
| OTA_0 / OTA_1 | 3 MB each | Dual application slots with rollback |
| LittleFS | 2 MB | SSH keys, known_hosts, dashboard config |
| Core Dump | 1 MB | Crash diagnostics |

#### Building

Requires ESP-IDF v5.5+ and the [1bit-display](https://github.com/tinkeringtanuki/1bit-display) library:

```bash
cd esp32_rendering

# Set the path to the 1bit-display library (default: ../../1bit-display relative to esp32_rendering/)
export ONEBIT_LIB_DIR=/path/to/1bit-display

idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

#### First-Time Setup

1. **Flash firmware** and power on
2. **Pair a BLE keyboard** -- long-press Button A to enter pairing mode (30s timeout)
3. **Connect WiFi** -- the device auto-connects to networks saved in NVS or in `/sdcard/wifi.json`. Adding a new network from the UI is not implemented.
4. **Configure SSH** -- drop a JSON file into `/sdcard/servers/` with `{name, host, port, username, dashboard[]}`, then pick it from the Servers menu. (In-app Settings editor is not yet implemented.)
5. **Authenticate** -- password on first connect; upload Ed25519 keys to `/littlefs/ssh_ed25519` for key auth

#### Dashboard Configuration

Create `/littlefs/dashboard.cfg` with one command per line (label|command format):

```
CPU|cat /proc/loadavg
Memory|free -m
Disk|df -h
GPU|cat /sys/class/drm/card0/device/gpu_busy_percent
Screens|screen -ls
```

Falls back to built-in defaults if no config file exists.

### Simulator

A Python/Pygame simulator for prototyping 1-bit display designs before deploying to hardware. Includes a rendering toolkit inspired by Lucas Pope's Mars After Midnight visual techniques.

#### Features

- **Portable rendering core** - Pure Python modules that map cleanly to C++ for ESP32 porting
- **Optimized 1-bit framebuffer** - Byte-aligned operations for efficient rendering
- **Drawing primitives** - Bresenham lines, scanline polygon fill, midpoint circles
- **Bayer dithering** - 5 pattern levels for visual texture (0%, 25%, 50%, 75%, 100%)
- **Bezier curves** - Cubic beziers with auto-smooth tangents and texture-ball strokes
- **Vector typography** - Full A-Z alphabet, numerals 0-9, punctuation with scalable stroke width
- **Animation system** - Breathing, wiggle, and transition effects for organic movement
- **Layout helpers** - Centered, right-aligned, and multi-line text rendering

#### Running the Simulator

```bash
cd simulator
python -m venv venv
source venv/bin/activate  # or `venv\Scripts\activate` on Windows
pip install -r requirements.txt
python main.py --scale 2
```

**Controls:**
- `SPACE` - Cycle through demo modes
- `1-5` - Jump to specific mode
- `A` - Toggle animation effects
- `S` - Save screenshot
- `Q` / `ESC` - Quit

**Demo Modes:**
1. **Patterns** - All 5 dither patterns in hexagonal shapes (breathing animation)
2. **Bezier** - Organic curves with texture-ball strokes (wiggle animation)
3. **Numerals** - Full digit set at multiple sizes with live clock
4. **Clock Sketch** - Combined composition preview (wiggle + breathing)
5. **Typography** - Full A-Z alphabet, sample phrases, and mixed text

### hello_vu

ESP-IDF firmware implementing a dual-channel VU meter using the onboard microphone array.

#### Features

- Real-time audio level visualization
- Dual VU meters (left/right channels) with 16 segments each
- Automatic gain control with adaptive noise floor
- 20 FPS display refresh

#### Building

```bash
cd hello_vu
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure

```
RLCD/
├── esp32_rendering/            # SSH Terminal & Dashboard (main project)
│   ├── main/                   # Application entry point and boot sequence
│   ├── components/
│   │   ├── st7305/             # ST7305 reflective LCD driver (SPI + DMA)
│   │   ├── rendering/          # Graphics primitives (clock face, shapes)
│   │   ├── wifi_manager/       # WiFi lifecycle, NVS credentials, auto-connect
│   │   ├── ssh_client/         # libssh2_esp SSH with optimized cipher suites
│   │   ├── ble_hid/            # NimBLE BLE keyboard HID host
│   │   ├── input_queue/        # Unified FreeRTOS event queue
│   │   ├── app/                # Menu, dashboard, terminal mode, settings
│   │   ├── buttons/            # Debounced button handler
│   │   ├── sensors/            # RTC, temperature/humidity, battery
│   │   └── i2c_bsp/            # I2C bus abstraction
│   ├── sdkconfig.defaults      # ESP32-S3 optimized config
│   └── partitions.csv          # 16MB flash layout with OTA + LittleFS
├── simulator/                  # Python/Pygame display simulator
├── hello_vu/                   # ESP-IDF VU meter project
├── REFERENCES/                 # Component datasheets
└── README.md
```

## Development Environment

This repository uses:
- **Python 3.9+** with Pygame for the simulator
- **ESP-IDF v5.x** for firmware development

### Prerequisites

- [Python 3.9+](https://www.python.org/downloads/)
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) v5.x or later
- VS Code with ESP-IDF extension (recommended)

## Test Infrastructure

Two complementary test surfaces:

### Host unit tests (Catch2)

Pure-logic unit tests under `host_test/app/` run without hardware.

~~~bash
cd host_test
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
~~~

### On-device scenarios

The pytest + pyserial harness at `host_test/scenarios/` drives real
firmware over the ESP32-S3's built-in USB-JTAG CDC via the
`test_console` REPL component — ~40 commands covering input
injection, NVS, filesystem, introspection, and runtime control.
14 scenarios cover boot, menu cycling, font settings, pairing,
WiFi, servers, settings persistence, and the legacy-NVS migration
path. See `host_test/scenarios/README.md` for fixture docs, the
full command set, and concurrency caveats.

Build the test-overlay firmware and run the suite:

~~~bash
cd esp32_rendering
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test" \
       -p /dev/cu.usbmodem4101 flash
cd ../host_test/scenarios
pip install -r requirements.txt
export TEST_WIFI_SSID='your-ssid' TEST_WIFI_PASS='your-pass'
python -m pytest -v
~~~

The test overlay routes the primary console to USB-JTAG CDC
(default production builds use UART0) and enables FreeRTOS trace
facilities for the `tasklist` REPL command. Production firmware
has zero test-console code — the component is gated by
`CONFIG_TEST_CONSOLE_ENABLED` and produces empty sources when
disabled.

## Reference Documentation

### Waveshare Resources
- [Product Page](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)
- [Wiki (English)](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2)
- [Documentation](https://docs.waveshare.com/ESP32-S3-RLCD-4.2)
- [GitHub Examples](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2)

### Design Inspiration
- [Mars After Midnight - Working in One Bit](https://dukope.itch.io/mars-after-midnight/devlog/285964/working-in-one-bit) - Lucas Pope's devlog on 1-bit graphics techniques

### Espressif Resources
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

## License

See [LICENSE](LICENSE) file.
