# ESP32-S3 Rendering Toolkit

A high-performance C++ rendering library for the Waveshare ESP32-S3-RLCD-4.2 board with ST7305 reflective LCD (400x300, 1-bit).

## Features

- **Hardware-agnostic rendering library** - Can be used with any 1-bit display
- **Optimized for ESP32-S3** - Uses PSRAM for large buffers, efficient byte operations
- **Full primitives suite** - Lines, polygons, circles, rectangles with fill support
- **Bayer dithering** - 5 pattern levels for grayscale simulation
- **Bezier curves** - Cubic bezier with texture-ball strokes (Pope's technique)
- **Vector font** - Scalable 0-9, A-Z, punctuation with variable widths
- **Animation utilities** - Easing, breathing, wiggle, morphing effects

## Project Structure

```
esp32_rendering/
├── components/
│   ├── rendering/          # Hardware-agnostic rendering library
│   │   ├── include/rendering/
│   │   │   ├── types.hpp       # Point, PointF, Rect, Color
│   │   │   ├── framebuffer.hpp # Template framebuffer
│   │   │   ├── primitives.hpp  # Lines, polygons, circles
│   │   │   ├── patterns.hpp    # Bayer dithering
│   │   │   ├── bezier.hpp      # Cubic bezier curves
│   │   │   ├── vector_font.hpp # Scalable font
│   │   │   └── animation.hpp   # Animation utilities
│   │   └── src/
│   └── st7305/             # Display driver
│       ├── include/st7305.hpp
│       └── src/st7305.cpp
├── main/
│   └── main.cpp            # Demo application
├── CMakeLists.txt
├── sdkconfig.defaults
└── partitions.csv
```

## Quick Start

### Prerequisites

- **ESP-IDF v5.5.x** (required for new I2C master driver API)
- Waveshare ESP32-S3-RLCD-4.2 board

### Installing ESP-IDF

**Option 1: ESP-IDF Extension Manager (Recommended)**
- Install the [VS Code ESP-IDF extension](https://github.com/espressif/vscode-esp-idf-extension)
- Or download from: https://github.com/espressif/idf-installer

**Option 2: Manual Installation**
```bash
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git esp-idf-v5.5
cd esp-idf-v5.5
./install.sh esp32s3
```

### Build and Flash

Use the included wrapper script (auto-detects ESP-IDF installation):

```bash
cd esp32_rendering
./idf.sh build              # Build
./idf.sh flash              # Flash to device
./idf.sh monitor            # Serial monitor
./idf.sh flash monitor      # Flash and monitor
./idf.sh menuconfig         # Configure options
```

Or manually with ESP-IDF:
```bash
source ~/.espressif/v5.5.2/esp-idf/export.sh  # Path may vary
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## API Overview

### Framebuffer

```cpp
#include "rendering/framebuffer.hpp"

rendering::Framebuffer400x300 fb;
fb.clear(rendering::WHITE);
fb.setPixel(100, 50, rendering::BLACK);
fb.fillSpan(y, x0, x1, rendering::BLACK);  // x1 is exclusive
```

### Primitives

```cpp
#include "rendering/primitives.hpp"

using namespace rendering;

drawLine(fb, 0, 0, 100, 100, BLACK);
drawThickLine(fb, p0, p1, 3, BLACK);
fillPolygon(fb, points, count, BLACK);
fillCircle(fb, 200, 150, 50, BLACK);
fillRect(fb, 10, 10, 80, 60, BLACK);
```

### Patterns (Dithering)

```cpp
#include "rendering/patterns.hpp"

fillPolygonPattern(fb, points, count, Pattern::Medium);
fillCirclePattern(fb, cx, cy, r, Pattern::Dense);
fillRectPattern(fb, x, y, w, h, Pattern::Sparse);
```

### Bezier Curves

```cpp
#include "rendering/bezier.hpp"

PointF curve[] = {{20, 50}, {100, 20}, {180, 80}, {260, 40}};
drawBezierCurve(fb, curve, 4, 0.5f, BLACK);
strokeBezierTextureBall(fb, curve, 4, 0.5f, 3.0f);  // Textured stroke
```

### Vector Font

```cpp
#include "rendering/vector_font.hpp"

renderString(fb, "HELLO", 10, 20, 30, 40, 4, 2, BLACK);
renderStringCentered(fb, "CENTERED", 200, 100, 24, 32);
renderStringRight(fb, "RIGHT", 390, 200, 20, 28);
```

### Animation

```cpp
#include "rendering/animation.hpp"

AnimationState anim(getTime());
anim.update(currentTime);

float scale = anim.breathingScale(0.9f, 1.1f, 2.0f);
float offset = anim.breathingOffset(10.0f, 1.5f);

Point wiggled[6];
wigglePoints(points, 6, wiggled, 3.0f, 5.0f, anim.elapsed(), 12345);

PointF morphed[5];
transitionPoints(shapeA, shapeB, 5, morphed, progress, easeInOut);
```

### Display Driver

```cpp
#include "st7305.hpp"

st7305::Config config;  // Uses default pins
st7305::Display display(config);
display.init();
display.show(fb);  // Transfer framebuffer to display
```

## ST7305 Display Configuration

The ST7305 reflective LCD requires specific initialization settings to prevent flickering with dithered patterns. The driver uses the official Waveshare BSP settings:

### Critical Settings

| Register | Value | Purpose |
|----------|-------|---------|
| 0xC1 (VSHP) | 0x41 | Source driving voltage |
| 0xC4 | 0x41 | Voltage setting |
| 0xD8 | 0xA6, 0xE9 | Panel timing control |
| 0xB2 | 0x05 | Booster setting |
| 0xB0 | 0x64 | Frequency setting |
| 0x21 | - | **Display Inversion ON** |

### Display Inversion Mode

The display **must** use Display Inversion ON (0x21) to prevent flickering with high-frequency patterns like Bayer dithering or checkerboards. This requires inverting the pixel logic in the driver:

- Buffer initialized to 0xFF (white background)
- BLACK pixels: clear the corresponding bit
- WHITE pixels: bit remains set

See `docs/ST7305_FLICKER_INVESTIGATION.md` for detailed analysis of the flicker issue and solution.

## Memory Usage

| Component | Size | Location |
|-----------|------|----------|
| Framebuffer | 15 KB | PSRAM |
| Display buffer | 15 KB | PSRAM |
| Pixel LUTs | 360 KB | PSRAM |
| Glyph data | ~3 KB | Flash |
| **Total PSRAM** | ~390 KB | 4.8% of 8MB |

## Pin Configuration (Default)

| Signal | GPIO | Notes |
|--------|------|-------|
| SPI MOSI | 12 | LCD data |
| SPI SCK | 11 | LCD clock |
| LCD DC | 5 | Data/Command |
| LCD CS | 40 | Chip select |
| LCD RST | 41 | Reset |
| I2C SDA | 6 | RTC, Temp sensor |
| I2C SCL | 7 | RTC, Temp sensor |
| BAT_ADC | 4 | ADC1_CH3, 3:1 divider |

## License

MIT License
