# Hello VU Project Design

## Overview

ESP-IDF project for the Waveshare ESP32-S3-RLCD-4.2 that displays "JP LISTENNING DEVICE" centered on screen with VU meters on left and right edges that react to dual microphone input.

## Hardware

- Display: 400×300 1-bit monochrome RLCD, ST7305 driver, SPI
- Audio: ES7210 ADC with dual microphones (stereo input)
- MCU: ESP32-S3-WROOM-1-N16R8

### Pin Assignments

| Function | GPIO |
|----------|------|
| SPI MOSI | 12 |
| SPI SCK | 11 |
| LCD DC | 5 |
| LCD CS | 40 |
| LCD RST | 41 |
| I2C SDA | 13 |
| I2C SCL | 14 |

## Display Layout

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│  ░░░░                                              ░░░░  │
│  ░░░░                                              ░░░░  │
│  ░░░░                                              ░░░░  │
│  ░░░░         JP LISTENNING DEVICE                 ░░░░  │
│  ████                                              ████  │
│  ████                                              ████  │
│  ████                                              ████  │
│  ████                                              ████  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### VU Meter Specs

- 8 segments per meter, filling bottom to top
- Each segment: 30px wide × 28px tall, 4px gap
- Total height: 252px (8 × 28 + 7 × 4)
- Left meter: 20px from left edge
- Right meter: 20px from right edge (x = 350)
- Filled = black, empty = white

### Text

- "JP LISTENNING DEVICE" centered at (200, 150)
- 8×16 bitmap font

## Audio Processing

- Sample rate: 16kHz, 16-bit stereo
- Read 512 bytes per frame (~8ms)
- Calculate RMS per channel
- Smoothing: 50ms attack, 300ms decay
- Map to 0-8 segment range

## Program Flow

```
app_main()
├── Init I2C bus
├── Init display
├── Init codec (ES7210)
├── Draw static text
├── Start audio_task
└── Start display_task

audio_task (priority 5)
└── Read mics → calc RMS → update levels

display_task (priority 3)
└── Draw VU meters at 20fps
```

## Project Structure

```
hello_vu/
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   └── font8x16.h
└── components/
    └── bsp/
        ├── CMakeLists.txt
        ├── include/
        │   ├── display_bsp.h
        │   ├── codec_bsp.h
        │   └── i2c_bsp.h
        └── src/
            ├── display_bsp.cpp
            ├── codec_bsp.cpp
            └── i2c_bsp.cpp
```

## Dependencies

- ESP-IDF v5.x
- esp_codec_dev component (from Espressif component registry)
- No external graphics library
