# RLCD

Playground for building and testing different setups for the Waveshare ESP32-S3-RLCD-4.2 development board.

## Board Overview

The ESP32-S3-RLCD-4.2 is a development board featuring a 4.2" reflective LCD that requires no backlight.

| Component | Specification |
|-----------|---------------|
| **Display** | 4.2" RLCD, 300×400 pixels, 1-bit monochrome, SPI (ST7305 driver) |
| **SoC** | ESP32-S3-WROOM-1-N16R8 (dual-core Xtensa LX7 @ 240MHz) |
| **Memory** | 16MB Flash, 8MB PSRAM, 512KB SRAM |
| **Connectivity** | WiFi 2.4GHz, Bluetooth 5 LE |
| **Audio** | ES8311 codec, ES7210 ADC with dual-mic array |
| **Sensors** | SHTC3 (temperature/humidity) |
| **RTC** | PCF85063 |
| **Power** | 18650 battery holder, USB-C |
| **Storage** | TF card slot (FAT32) |

## Development Environment

This repository primarily uses **ESP-IDF** (Espressif IoT Development Framework).

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) v5.x or later
- VS Code with ESP-IDF extension (recommended)

### Building a Project

```bash
cd <project_directory>
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure

```
RLCD/
├── REFERENCES/          # Component datasheets and schematics
│   ├── ES8311.DS.pdf                    # Audio codec
│   ├── ESP32-S3-RLCD-4.2-schematic.pdf  # Board schematic
│   ├── Pcf85063atl1118-*.pdf            # RTC chip
│   ├── SHTC3_Datasheet.pdf              # Temperature/humidity sensor
│   ├── ST_7305_V0_2.pdf                 # Display driver IC
│   └── esp32-s3_technical_reference_manual_en.pdf
└── README.md
```

## Reference Documentation

### Waveshare Resources
- [Product Page](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)
- [Wiki (English)](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2)
- [Documentation](https://docs.waveshare.com/ESP32-S3-RLCD-4.2)
- [GitHub Examples](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2)

### ESP-IDF Examples from Waveshare
The official repository includes ESP-IDF examples for:
- WiFi (AP and STA modes)
- ADC testing
- I2C peripherals (PCF85063 RTC, SHTC3 sensor)
- SD card operations
- Audio testing
- LVGL graphics (v8 and v9)

### Espressif Resources
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

## License

See [LICENSE](LICENSE) file.
