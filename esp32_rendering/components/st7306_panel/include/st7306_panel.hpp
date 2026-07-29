#pragma once

#include <1bit/hal/st7306_display.hpp>

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace board {

/// Pin/bus configuration for the ST7306-class reflective panel on the
/// Waveshare ESP32-S3-RLCD-4.2.
///
/// These defaults are the shipping, hardware-proven map carried over verbatim
/// from the driver this component replaces. Do not "tidy" them.
struct St7306Pins {
    int mosi = 12;
    int sclk = 11;
    int dc   = 5;
    int cs   = 40;
    int rst  = 41;
    int width  = 400;
    int height = 300;
    int spiClockHz = 10 * 1000 * 1000;  // 10 MHz, as shipped
    spi_host_device_t spiHost = SPI2_HOST;
};

/// Transport-only subclass of onebit::St7306Display.
///
/// Everything that decides *which bytes* reach the panel -- the init sequence,
/// the address window, the 2x4-block scan-out transform and the inversion
/// polarity -- lives in the onebit library and is host-tested there
/// (tests/hal/test_st7306_display.cpp, test_st7306_transform.cpp and the
/// differential gate in test_st7306_rlcd_equivalence.cpp, which proves this
/// path is byte-identical to the driver it replaces). This class supplies only
/// *how* those bytes get there.
class St7306Panel : public onebit::St7306Display {
public:
    explicit St7306Panel(const St7306Pins& pins = St7306Pins{});
    ~St7306Panel() override;

    St7306Panel(const St7306Panel&) = delete;
    St7306Panel& operator=(const St7306Panel&) = delete;

    /// Bring up SPI, hard-reset the panel, run the library's command sequence
    /// and clear to paper. Idempotent.
    ///
    /// NOTE: deliberately hides the non-virtual onebit::St7306Display::init(),
    /// which it calls internally. Nothing outside this class should call the
    /// base version -- doing so leaves the panel unreset and unclear.
    esp_err_t init();

    bool initialized() const { return initialized_; }

    /// Block until the last queued pixel DMA has drained.
    void waitTxDone();

    /// The scan-out buffer is rewritten in place by writeRegion(), and
    /// esp_lcd_panel_io_tx_color() only *queues* the transfer. Draining here --
    /// before the conversion, not after -- is what keeps the frame in flight
    /// intact. The driver this replaces did not do this and could overwrite a
    /// buffer the DMA engine was still reading.
    void beginFrame() override { waitTxDone(); }

protected:
    void sendCommand(uint8_t cmd) override;
    void sendData(const uint8_t* data, size_t len) override;
    void sendPixels(const uint8_t* data, size_t len) override;
    void delayMs(uint32_t ms) override;

private:
    void hardReset();
    static bool onColorDone(esp_lcd_panel_io_handle_t io,
                            esp_lcd_panel_io_event_data_t* edata,
                            void* ctx);

    St7306Pins pins_;
    esp_lcd_panel_io_handle_t io_ = nullptr;
    SemaphoreHandle_t txDone_ = nullptr;
    bool txPending_ = false;
    bool initialized_ = false;
};

} // namespace board
