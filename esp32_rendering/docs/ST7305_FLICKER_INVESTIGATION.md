# ST7305 Display Flicker Investigation

## Summary

Investigation of flickering issues on the Waveshare ESP32-S3-RLCD-4.2 development board with ST7305 reflective LCD controller.

## Key Finding

**The flicker is a display hardware issue, not a rendering code bug.** The ST7305 reflective LCD has specific sensitivity to certain pixel patterns depending on its configuration.

## Diagnostic Results

| Test Pattern | Uses | Flickers? |
|-------------|------|-----------|
| SOLID (black rectangle) | fillSpan | No |
| DITHER (Bayer 4x4) | setPixel | Yes* |
| CHECK (checkerboard via fillSpan) | fillSpan | Yes* |
| CHECK (checkerboard via setPixel) | setPixel | Yes* |
| STRIPE (horizontal lines) | fillSpan | No* |

*Flicker behavior depends on display initialization settings

## Critical Discovery

The checkerboard pattern created with `fillSpan` also flickers, proving the issue is **not** with `setPixel()` or the rendering code. The display hardware itself has trouble with certain high-frequency alternating pixel patterns.

## Frequency Setting (0xB8) Effects

| Value | Dither/Check | Stripes |
|-------|--------------|---------|
| 0x29 (original) | Flickers | OK |
| 0x09 | OK | Flickers |
| 0x19 | Mixed | Flickers |

## Official Waveshare BSP Comparison

Comparing our initialization with the [official Waveshare BSP](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2):

| Register | Our Value | Waveshare BSP | Purpose |
|----------|-----------|---------------|---------|
| 0xC1 (VSHP) | 0x69,0x69,0x69,0x69 | 0x41,0x41,0x41,0x41 | Source Driving Voltage |
| 0xC4 | 0x4B,0x4B,0x4B,0x4B | 0x41,0x41,0x41,0x41 | Voltage Setting |
| 0xD8 | 0x80,0xE9 | 0xA6,0xE9 | Panel Control/Timing |
| 0xB2 | 0x02 | 0x05 | Booster Setting |
| 0xB0 | (missing) | 0x64 | Frequency Setting |
| 0x20/0x21 | 0x20 (OFF) | 0x21 (ON) | **Display Inversion** |

### Key Differences

1. **Display Inversion**: Waveshare uses `0x21` (ON), we use `0x20` (OFF)
2. **Missing 0xB0 register**: Waveshare sets frequency via 0xB0 = 0x64
3. **Different voltage settings**: VSHP and other voltage registers differ significantly

## Initialization Sequence (Official Waveshare)

```cpp
// From Waveshare ESP32-S3-RLCD-4.2 BSP
void RLCD_Init() {
    // NVM Load Control
    sendCommand(0xD6); sendData(0x17); sendData(0x02);

    // Booster Enable
    sendCommand(0xD1); sendData(0x01);

    // Gate Voltage Control
    sendCommand(0xC0); sendData(0x11); sendData(0x04);

    // VSHP Setting (Source Driving Voltage)
    sendCommand(0xC1);
    sendData(0x41); sendData(0x41); sendData(0x41); sendData(0x41);

    // Voltage settings
    sendCommand(0xC2);
    sendData(0x19); sendData(0x19); sendData(0x19); sendData(0x19);

    sendCommand(0xC4);
    sendData(0x41); sendData(0x41); sendData(0x41); sendData(0x41);

    sendCommand(0xC5);
    sendData(0x19); sendData(0x19); sendData(0x19); sendData(0x19);

    // Timing control
    sendCommand(0xD8); sendData(0xA6); sendData(0xE9);

    // Display setting
    sendCommand(0xB2); sendData(0x05);

    // FRC/Waveform settings
    sendCommand(0xB3);
    sendData(0xE5); sendData(0xF6); sendData(0x05); sendData(0x46);
    sendData(0x77); sendData(0x77); sendData(0x77); sendData(0x77);
    sendData(0x76); sendData(0x45);

    sendCommand(0xB4);
    sendData(0x05); sendData(0x46); sendData(0x77); sendData(0x77);
    sendData(0x77); sendData(0x77); sendData(0x76); sendData(0x45);

    // Timing
    sendCommand(0x62);
    sendData(0x32); sendData(0x03); sendData(0x1F);

    // Display control
    sendCommand(0xB7); sendData(0x13);

    // Frequency setting (IMPORTANT - not in our code!)
    sendCommand(0xB0); sendData(0x64);

    // Sleep Out
    sendCommand(0x11);
    delay(200);

    // Register setting
    sendCommand(0xC9); sendData(0x00);

    // Memory Access Control
    sendCommand(0x36); sendData(0x48);

    // Interface Pixel Format (4-bit)
    sendCommand(0x3A); sendData(0x11);

    // Duty and Frequency
    sendCommand(0xB9); sendData(0x20);
    sendCommand(0xB8); sendData(0x29);

    // Display Inversion ON (IMPORTANT!)
    sendCommand(0x21);

    // Column/Row range
    sendCommand(0x2A); sendData(0x12); sendData(0x2A);
    sendCommand(0x2B); sendData(0x00); sendData(0xC7);

    // Tearing Effect
    sendCommand(0x35); sendData(0x00);

    // Power control
    sendCommand(0xD0); sendData(0xFF);

    // Idle Mode Off, Display On
    sendCommand(0x38);
    sendCommand(0x29);
}
```

## Solution Applied

The fix required matching the official Waveshare BSP initialization sequence:

1. **VSHP Setting (0xC1)**: Changed from `0x69` to `0x41`
2. **Voltage Setting (0xC4)**: Changed from `0x4B` to `0x41`
3. **Panel Timing (0xD8)**: Changed from `0x80, 0xE9` to `0xA6, 0xE9`
4. **Booster Setting (0xB2)**: Changed from `0x02` to `0x05`
5. **Display Inversion**: Set to ON (`0x21`)

### Handling Display Inversion

With Display Inversion ON, the pixel logic must be inverted in `convertToDisplayFormat()`:
- Start with all bits SET (0xFF) for white background
- CLEAR bits for BLACK pixels (instead of setting them)

```cpp
// With Display Inversion ON (0x21), clear bits = BLACK, set bits = WHITE
std::memset(displayBuffer_, 0xFF, bufferSize);  // White background
// ...
if (pixel) {  // BLACK pixel
    displayBuffer_[dstByteIdx] &= ~dstBit;  // Clear bit
}
```

Similarly, `Display::clear()` must invert:
```cpp
std::memset(displayBuffer_, color ? 0x00 : 0xFF, bufferSize);
```

## Resources

- [Waveshare ESP32-S3-RLCD-4.2 GitHub](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2)
- [Waveshare Documentation](https://docs.waveshare.com/ESP32-S3-RLCD-4.2)
- [ESPHome ST7305 Custom Component](https://community.home-assistant.io/t/custom-component-for-waveshare-esp32-s3-4-2-rlcd-st7305/982089)
- [CNX Software Article](https://www.cnx-software.com/2026/01/06/esp32-s3-development-board-features-4-2-inch-reflective-lcd-rlcd-dual-microphone-array-onboard-speaker/)
