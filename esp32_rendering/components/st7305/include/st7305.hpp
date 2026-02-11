#pragma once

#include "rendering/framebuffer.hpp"
#include <esp_lcd_panel_io.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <cstdint>

namespace st7305 {

/// Pin configuration for ST7305 display
struct Config {
    int mosi = 12;
    int sclk = 11;
    int dc = 5;
    int cs = 40;
    int rst = 41;
    int width = 400;
    int height = 300;
    int spiClockHz = 10 * 1000 * 1000;  // 10 MHz
    spi_host_device_t spiHost = SPI2_HOST;
};

/// ST7305 display driver for Waveshare ESP32-S3-RLCD-4.2
class Display {
public:
    explicit Display(const Config& config = Config{});
    ~Display();

    // Non-copyable
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    /// Initialize display hardware
    void init();

    /// Transfer framebuffer to display
    /// Handles conversion from row-major to ST7305 LUT format
    void show(const rendering::IFramebuffer& fb);

    /// Direct clear (bypasses framebuffer)
    void clear(bool color = false);

    int width() const { return config_.width; }
    int height() const { return config_.height; }

private:
    Config config_;
    esp_lcd_panel_io_handle_t ioHandle_;
    uint8_t* displayBuffer_;  // ST7305 format buffer
    uint16_t* pixelIndexLUT_; // LUT for pixel byte index [x][y]
    uint8_t* pixelBitLUT_;    // LUT for pixel bit mask [x][y]
    bool initialized_;

    void initLUT();
    void initSPI();
    void initDisplay();
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void sendBuffer(const uint8_t* data, size_t len);
    void reset();

    /// Convert framebuffer to ST7305 format using LUT
    void convertToDisplayFormat(const rendering::IFramebuffer& fb);

    /// LUT index helper
    size_t lutIndex(int x, int y) const {
        return static_cast<size_t>(x) * config_.height + y;
    }
};

} // namespace st7305
