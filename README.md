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

### Simulator

A Python/Pygame simulator for prototyping 1-bit display designs before deploying to hardware. Includes a rendering toolkit inspired by Lucas Pope's Mars After Midnight visual techniques.

#### Features

- **Portable rendering core** - Pure Python modules that map cleanly to C++ for ESP32 porting
- **Optimized 1-bit framebuffer** - Byte-aligned operations for efficient rendering
- **Drawing primitives** - Bresenham lines, scanline polygon fill, midpoint circles
- **Bayer dithering** - 5 pattern levels for visual texture (0%, 25%, 50%, 75%, 100%)
- **Bezier curves** - Cubic beziers with auto-smooth tangents and texture-ball strokes
- **Vector typography** - Geometric numerals (0-9) with scalable stroke width

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
- `1-4` - Jump to specific mode
- `S` - Save screenshot
- `Q` / `ESC` - Quit

**Demo Modes:**
1. **Patterns** - All 5 dither patterns in hexagonal shapes
2. **Bezier** - Organic curves with texture-ball strokes
3. **Numerals** - Full digit set at multiple sizes with live clock
4. **Clock Sketch** - Combined composition preview

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
├── simulator/              # Python display simulator
│   ├── rendering/          # Portable rendering toolkit
│   │   ├── framebuffer.py  # 1-bit packed pixel buffer
│   │   ├── primitives.py   # Lines, polygons, circles
│   │   ├── patterns.py     # Bayer dither patterns
│   │   ├── bezier.py       # Curves and texture-ball strokes
│   │   ├── vector_font.py  # Geometric numerals
│   │   └── display.py      # Pygame visualization
│   ├── demo.py             # 4-mode interactive showcase
│   └── main.py             # Entry point
├── hello_vu/               # ESP-IDF VU meter project
│   ├── main/
│   │   └── main.cpp        # Application code
│   └── components/bsp/     # Board support package
├── docs/
│   └── plans/              # Design documents
├── REFERENCES/             # Component datasheets
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
