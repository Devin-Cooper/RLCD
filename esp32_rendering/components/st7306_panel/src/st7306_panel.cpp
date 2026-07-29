#include "st7306_panel.hpp"

#include <esp_log.h>
#include <freertos/task.h>

#include <cstring>

namespace board {

namespace {
constexpr char TAG[] = "st7306";
} // namespace

St7306Panel::St7306Panel(const St7306Pins& pins)
    : onebit::St7306Display(
          onebit::St7306Layout(static_cast<int16_t>(pins.width),
                               static_cast<int16_t>(pins.height)))
    , pins_(pins) {}

St7306Panel::~St7306Panel() {
    // Must drain before ~St7306Display frees the scan-out buffer: base
    // destructors run after this body, and DMA may still be reading it.
    waitTxDone();
    if (io_) esp_lcd_panel_io_del(io_);
    if (txDone_) vSemaphoreDelete(txDone_);
}

bool St7306Panel::onColorDone(esp_lcd_panel_io_handle_t,
                              esp_lcd_panel_io_event_data_t*,
                              void* ctx) {
    auto* self = static_cast<St7306Panel*>(ctx);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(self->txDone_, &woken);
    return woken == pdTRUE;
}

esp_err_t St7306Panel::init() {
    if (initialized_) return ESP_OK;

    // The base constructor allocated the scan-out buffer through onebit::alloc,
    // so onebit::init() must already have run. A failure here is 15 KB of
    // DMA-capable internal SRAM the heap could not find.
    if (!isValid()) {
        ESP_LOGE(TAG, "scan-out buffer alloc failed (%u bytes)",
                 static_cast<unsigned>(layout().bufferSize()));
        return ESP_ERR_NO_MEM;
    }

    // One tx_color per frame, and esp_lcd raises the done callback only on the
    // final chunk, so a binary semaphore is exactly right.
    txDone_ = xSemaphoreCreateBinary();
    if (!txDone_) return ESP_ERR_NO_MEM;

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = -1;  // write-only panel, no readback
    buscfg.mosi_io_num = pins_.mosi;
    buscfg.sclk_io_num = pins_.sclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = static_cast<int>(layout().bufferSize());
    ESP_ERROR_CHECK(spi_bus_initialize(pins_.spiHost, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = pins_.dc;
    io_config.cs_gpio_num = pins_.cs;
    io_config.pclk_hz = pins_.spiClockHz;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = &St7306Panel::onColorDone;
    io_config.user_ctx = this;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        static_cast<esp_lcd_spi_bus_handle_t>(pins_.spiHost), &io_config, &io_));

    hardReset();

    // Library-owned, host-tested command sequence.
    onebit::St7306Display::init();

    // The driver this replaces ended its init sequence with clear(false);
    // onebit's init() stops at display-on and leaves clearing to the caller.
    // Without this the panel comes up showing whatever was in frame memory.
    clear(onebit::WHITE);

    initialized_ = true;
    ESP_LOGI(TAG, "init ok: %dx%d, SPI %d Hz, scan-out %u B",
             width(), height(), pins_.spiClockHz,
             static_cast<unsigned>(layout().bufferSize()));
    return ESP_OK;
}

void St7306Panel::hardReset() {
    gpio_config_t g = {};
    g.intr_type = GPIO_INTR_DISABLE;
    g.mode = GPIO_MODE_OUTPUT;
    g.pin_bit_mask = 1ULL << pins_.rst;
    g.pull_down_en = GPIO_PULLDOWN_DISABLE;
    g.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&g));

    // Timing carried over from the shipping driver (idle high, low 50 ms,
    // high 200 ms), NOT from the vendor demo's 50/20/50. This is the sequence
    // that has actually brought this panel up.
    const gpio_num_t rst = static_cast<gpio_num_t>(pins_.rst);
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void St7306Panel::waitTxDone() {
    if (!txPending_) return;
    xSemaphoreTake(txDone_, portMAX_DELAY);
    txPending_ = false;
}

void St7306Panel::sendCommand(uint8_t cmd) {
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, cmd, nullptr, 0));
}

void St7306Panel::sendData(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    // lcd_cmd = -1 means "no command byte; D/C high for the whole payload".
    //
    // Copied onto the stack first: the library's init tables are constexpr and
    // therefore live in flash, which is not DMA-capable. spi_master would
    // otherwise bounce each one through a heap_caps_aligned_alloc; a stack copy
    // is cheaper and keeps the heap off the init path. The largest parameter
    // block in the sequence is 10 bytes (0xB3), so the fallback below is dead
    // code today -- it exists so an oversized block degrades to IDF's bounce
    // buffer rather than being silently truncated or split across CS edges.
    uint8_t tmp[16];
    if (len <= sizeof(tmp)) {
        std::memcpy(tmp, data, len);
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, -1, tmp, len));
    } else {
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_, -1, data, len));
    }
}

void St7306Panel::sendPixels(const uint8_t* data, size_t len) {
    // Belt-and-braces for the clear() path, which push() does not bracket with
    // beginFrame()/endFrame().
    waitTxDone();
    if (esp_lcd_panel_io_tx_color(io_, -1, data, len) == ESP_OK) {
        txPending_ = true;  // asynchronous: DMA still owns `data`
    }
}

void St7306Panel::delayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace board
