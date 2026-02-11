#include "st7305.hpp"
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

static const char* TAG = "st7305";

namespace st7305 {

Display::Display(const Config& config)
    : config_(config)
    , ioHandle_(nullptr)
    , displayBuffer_(nullptr)
    , pixelIndexLUT_(nullptr)
    , pixelBitLUT_(nullptr)
    , initialized_(false) {
}

Display::~Display() {
    if (displayBuffer_) {
        heap_caps_free(displayBuffer_);
    }
    if (pixelIndexLUT_) {
        heap_caps_free(pixelIndexLUT_);
    }
    if (pixelBitLUT_) {
        heap_caps_free(pixelBitLUT_);
    }
    if (ioHandle_) {
        esp_lcd_panel_io_del(ioHandle_);
    }
}

void Display::init() {
    if (initialized_) return;

    ESP_LOGI(TAG, "Initializing ST7305 display %dx%d", config_.width, config_.height);

    // Allocate buffers in PSRAM
    size_t bufferSize = (config_.width * config_.height) / 8;
    size_t lutSize = static_cast<size_t>(config_.width) * config_.height;

    displayBuffer_ = static_cast<uint8_t*>(
        heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    pixelIndexLUT_ = static_cast<uint16_t*>(
        heap_caps_malloc(lutSize * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    pixelBitLUT_ = static_cast<uint8_t*>(
        heap_caps_malloc(lutSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!displayBuffer_ || !pixelIndexLUT_ || !pixelBitLUT_) {
        ESP_LOGE(TAG, "Failed to allocate display buffers");
        return;
    }

    ESP_LOGI(TAG, "Allocated display buffer: %zu bytes", bufferSize);
    ESP_LOGI(TAG, "Allocated LUTs: %zu bytes", lutSize * (sizeof(uint16_t) + 1));

    initLUT();
    initSPI();
    initDisplay();

    initialized_ = true;
    ESP_LOGI(TAG, "ST7305 initialization complete");
}

void Display::initLUT() {
    // Landscape mode LUT for ST7305
    // Memory layout: bytes organized as [byte_x][block_y] for landscape
    // Each byte contains 2 pixels horizontally (x coordinates)
    // Pixels are organized in vertical blocks of 4 lines

    int H4 = config_.height / 4;  // 300 / 4 = 75

    for (int y = 0; y < config_.height; y++) {
        int inv_y = config_.height - 1 - y;  // Invert Y (flip vertically)
        int block_y = inv_y / 4;             // Y block (0-74)
        int local_y = inv_y & 3;             // Y local (0-3)

        for (int x = 0; x < config_.width; x++) {
            int byte_x = x / 2;              // X byte position (2 pixels per byte)
            int local_x = x & 1;             // X local (0-1)

            // Calculate buffer index
            uint16_t index = static_cast<uint16_t>(byte_x * H4 + block_y);

            // Calculate bit position within byte (7 = MSB, 0 = LSB)
            uint8_t bit = 7 - ((local_y << 1) | local_x);

            size_t lutIdx = lutIndex(x, y);
            pixelIndexLUT_[lutIdx] = index;
            pixelBitLUT_[lutIdx] = (1 << bit);
        }
    }

    ESP_LOGI(TAG, "LUT initialized for landscape mode");
}

void Display::initSPI() {
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = -1;
    buscfg.mosi_io_num = config_.mosi;
    buscfg.sclk_io_num = config_.sclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = config_.width * config_.height / 8;

    ESP_ERROR_CHECK(spi_bus_initialize(config_.spiHost, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = config_.dc;
    io_config.cs_gpio_num = config_.cs;
    io_config.pclk_hz = config_.spiClockHz;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        reinterpret_cast<esp_lcd_spi_bus_handle_t>(config_.spiHost),
        &io_config, &ioHandle_));

    // Configure reset GPIO
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = (1ULL << config_.rst);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    gpio_set_level(static_cast<gpio_num_t>(config_.rst), 1);

    ESP_LOGI(TAG, "SPI initialized at %d Hz", config_.spiClockHz);
}

void Display::reset() {
    gpio_set_level(static_cast<gpio_num_t>(config_.rst), 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(static_cast<gpio_num_t>(config_.rst), 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void Display::sendCommand(uint8_t cmd) {
    esp_lcd_panel_io_tx_param(ioHandle_, cmd, nullptr, 0);
}

void Display::sendData(uint8_t data) {
    esp_lcd_panel_io_tx_param(ioHandle_, -1, &data, 1);
}

void Display::sendBuffer(const uint8_t* data, size_t len) {
    esp_lcd_panel_io_tx_color(ioHandle_, -1, data, len);
}

void Display::initDisplay() {
    reset();

    // ST7305 initialization sequence from Waveshare BSP
    sendCommand(0xD6);
    sendData(0x17);
    sendData(0x02);

    sendCommand(0xD1);
    sendData(0x01);

    sendCommand(0xC0);
    sendData(0x11);
    sendData(0x04);

    // Gate Driving Voltage
    sendCommand(0xC1);
    sendData(0x69);
    sendData(0x69);
    sendData(0x69);
    sendData(0x69);

    // Source Driving Voltage
    sendCommand(0xC2);
    sendData(0x19);
    sendData(0x19);
    sendData(0x19);
    sendData(0x19);

    sendCommand(0xC4);
    sendData(0x4B);
    sendData(0x4B);
    sendData(0x4B);
    sendData(0x4B);

    sendCommand(0xC5);
    sendData(0x19);
    sendData(0x19);
    sendData(0x19);
    sendData(0x19);

    // Panel Control
    sendCommand(0xD8);
    sendData(0x80);
    sendData(0xE9);

    // Booster Setting
    sendCommand(0xB2);
    sendData(0x02);

    // Waveform Timing 1
    sendCommand(0xB3);
    sendData(0xE5);
    sendData(0xF6);
    sendData(0x05);
    sendData(0x46);
    sendData(0x77);
    sendData(0x77);
    sendData(0x77);
    sendData(0x77);
    sendData(0x76);
    sendData(0x45);

    // Waveform Timing 2
    sendCommand(0xB4);
    sendData(0x05);
    sendData(0x46);
    sendData(0x77);
    sendData(0x77);
    sendData(0x77);
    sendData(0x77);
    sendData(0x76);
    sendData(0x45);

    // Display Function Control
    sendCommand(0x62);
    sendData(0x32);
    sendData(0x03);
    sendData(0x1F);

    // FRC & PDW Control
    sendCommand(0xB7);
    sendData(0x13);

    // VDC Setting
    sendCommand(0xB0);
    sendData(0x64);

    // Sleep Out
    sendCommand(0x11);
    vTaskDelay(pdMS_TO_TICKS(200));

    // VCOM DC Setting
    sendCommand(0xC9);
    sendData(0x00);

    // Memory Access Control
    sendCommand(0x36);
    sendData(0x48);

    // Pixel Format (1-bit mode)
    sendCommand(0x3A);
    sendData(0x11);

    // Duty Setting
    sendCommand(0xB9);
    sendData(0x20);

    // Frequency Setting
    sendCommand(0xB8);
    sendData(0x29);

    // Display Inversion
    sendCommand(0x21);

    // Column Address Set
    sendCommand(0x2A);
    sendData(0x12);
    sendData(0x2A);

    // Row Address Set
    sendCommand(0x2B);
    sendData(0x00);
    sendData(0xC7);

    // Tearing Effect Line On
    sendCommand(0x35);
    sendData(0x00);

    // Gamma Setting
    sendCommand(0xD0);
    sendData(0xFF);

    // Idle Mode Off
    sendCommand(0x38);

    // Display On
    sendCommand(0x29);

    // Clear to white
    clear(false);

    ESP_LOGI(TAG, "Display initialized");
}

void Display::clear(bool color) {
    if (!displayBuffer_) return;

    size_t bufferSize = (config_.width * config_.height) / 8;
    std::memset(displayBuffer_, color ? 0xFF : 0x00, bufferSize);

    // Set column address
    sendCommand(0x2A);
    sendData(0x12);
    sendData(0x2A);

    // Set row address
    sendCommand(0x2B);
    sendData(0x00);
    sendData(0xC7);

    // Write memory
    sendCommand(0x2C);
    sendBuffer(displayBuffer_, bufferSize);
}

void Display::convertToDisplayFormat(const rendering::IFramebuffer& fb) {
    if (!displayBuffer_ || !pixelIndexLUT_ || !pixelBitLUT_) return;

    size_t bufferSize = (config_.width * config_.height) / 8;
    std::memset(displayBuffer_, 0x00, bufferSize);

    const uint8_t* srcBuffer = fb.buffer();
    int srcBytesPerRow = (fb.width() + 7) / 8;

    for (int y = 0; y < config_.height && y < fb.height(); y++) {
        for (int x = 0; x < config_.width && x < fb.width(); x++) {
            // Get pixel from framebuffer (row-major, MSB first)
            int srcByteIdx = y * srcBytesPerRow + (x >> 3);
            uint8_t srcBit = 1 << (7 - (x & 7));
            bool pixel = (srcBuffer[srcByteIdx] & srcBit) != 0;

            if (pixel) {
                // Set pixel in display buffer using LUT
                size_t lutIdx = lutIndex(x, y);
                uint16_t dstByteIdx = pixelIndexLUT_[lutIdx];
                uint8_t dstBit = pixelBitLUT_[lutIdx];
                displayBuffer_[dstByteIdx] |= dstBit;
            }
        }
    }
}

void Display::show(const rendering::IFramebuffer& fb) {
    if (!initialized_) {
        ESP_LOGW(TAG, "Display not initialized");
        return;
    }

    convertToDisplayFormat(fb);

    size_t bufferSize = (config_.width * config_.height) / 8;

    // Set column address
    sendCommand(0x2A);
    sendData(0x12);
    sendData(0x2A);

    // Set row address
    sendCommand(0x2B);
    sendData(0x00);
    sendData(0xC7);

    // Write memory
    sendCommand(0x2C);
    sendBuffer(displayBuffer_, bufferSize);
}

} // namespace st7305
