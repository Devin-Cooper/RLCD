// Recovery firmware display: renders "RECOVERY MODE" + build info on the
// ST7305 reflective LCD. Init block mirrors the production main.cpp so
// the two firmwares produce identical panel setup. A new shared
// display_bsp component is deliberately NOT introduced here; extracting
// one is a separate refactor.
#include "recovery_screen.hpp"

#include <1bit/core/allocator.hpp>
#include <1bit/core/framebuffer.hpp>
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/fonts/term_6x9.hpp>
#include <1bit/fonts/term_8x12.hpp>
#include "rendering/framebuffer.hpp"
#include "st7305.hpp"

#include <cstdio>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace recovery {

namespace {

static const char* TAG = "recovery_screen";

// Mirror the FramebufferAdapter from esp32_rendering/main/main.cpp so the
// ST7305 driver's rendering::IFramebuffer& API accepts our onebit::IFramebuffer.
class FramebufferAdapter : public rendering::IFramebuffer {
public:
    explicit FramebufferAdapter(onebit::IFramebuffer& src) : src_(src) {}

    int16_t width()  const override { return src_.width();  }
    int16_t height() const override { return src_.height(); }

    void setPixel(int16_t x, int16_t y, rendering::Color c) override {
        src_.setPixel(x, y, static_cast<onebit::Color>(c));
    }
    rendering::Color getPixel(int16_t x, int16_t y) const override {
        return static_cast<rendering::Color>(src_.getPixel(x, y));
    }
    void clear(rendering::Color c = rendering::WHITE) override {
        src_.clear(static_cast<onebit::Color>(c));
    }
    void setPixelDirect(int16_t x, int16_t y, rendering::Color c) override {
        src_.setPixelDirect(x, y, static_cast<onebit::Color>(c));
    }
    void fillSpan(int16_t y, int16_t xStart, int16_t xEnd, rendering::Color c) override {
        src_.fillSpan(y, xStart, xEnd, static_cast<onebit::Color>(c));
    }

    uint8_t* buffer() override { return src_.buffer(); }
    const uint8_t* buffer() const override { return src_.buffer(); }
    size_t bufferSize() const override { return src_.bufferSize(); }

private:
    onebit::IFramebuffer& src_;
};

const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:    return "power-on";
        case ESP_RST_EXT:        return "external";
        case ESP_RST_SW:         return "sw-restart";
        case ESP_RST_PANIC:      return "panic";
        case ESP_RST_INT_WDT:    return "int-wdt";
        case ESP_RST_TASK_WDT:   return "task-wdt";
        case ESP_RST_WDT:        return "other-wdt";
        case ESP_RST_DEEPSLEEP:  return "deep-sleep";
        case ESP_RST_BROWNOUT:   return "brownout";
        case ESP_RST_SDIO:       return "sdio";
        case ESP_RST_USB:        return "usb";
        case ESP_RST_JTAG:       return "jtag";
        case ESP_RST_EFUSE:      return "efuse";
        case ESP_RST_PWR_GLITCH: return "power-glitch";
        case ESP_RST_CPU_LOCKUP: return "cpu-lockup";
        case ESP_RST_UNKNOWN:
        default:                 return "unknown";
    }
}

// Display state — lives for the duration of the process. Framebuffer is
// in PSRAM via the onebit allocator; display handle owns the SPI bus.
onebit::Framebuffer<400, 300>* g_fb = nullptr;
st7305::Display* g_display = nullptr;

void render(onebit::IFramebuffer& fb, esp_reset_reason_t reason) {
    fb.clear(onebit::WHITE);

    // Outer + inner border so the screen looks distinctly different from
    // the main app's dashboard at a glance.
    onebit::drawRect(fb, 0, 0, fb.width(), fb.height(), onebit::BLACK);
    onebit::drawRect(fb, 2, 2, fb.width() - 4, fb.height() - 4, onebit::BLACK);
    onebit::drawRect(fb, 4, 4, fb.width() - 8, fb.height() - 8, onebit::BLACK);

    // Banner: "RECOVERY MODE" in the biggest font we have.
    const auto& bigFont = onebit::fonts::TERM_8X12;
    const char* banner = "RECOVERY MODE";
    int16_t bw = onebit::getBitmapTextWidth(bigFont, banner);
    onebit::drawBitmapText(fb, bigFont,
                            (fb.width() - bw) / 2, 40,
                            banner, onebit::BLACK);

    // Divider below banner.
    onebit::fillRect(fb, 20, 60, fb.width() - 40, 1, onebit::BLACK);

    // Body: build info + reset reason, using the smaller font for legibility.
    const auto& bodyFont = onebit::fonts::TERM_6X9;
    const esp_app_desc_t* desc = esp_app_get_description();

    char line1[64];
    std::snprintf(line1, sizeof(line1), "version: %s", desc ? desc->version : "?");
    onebit::drawBitmapText(fb, bodyFont, 20, 80, line1, onebit::BLACK);

    char line2[64];
    std::snprintf(line2, sizeof(line2), "built:   %s %s",
                  desc ? desc->date : "?", desc ? desc->time : "?");
    onebit::drawBitmapText(fb, bodyFont, 20, 95, line2, onebit::BLACK);

    char line3[64];
    std::snprintf(line3, sizeof(line3), "reset:   %s", resetReasonStr(reason));
    onebit::drawBitmapText(fb, bodyFont, 20, 110, line3, onebit::BLACK);

    // Instructions block.
    onebit::fillRect(fb, 20, 140, fb.width() - 40, 1, onebit::BLACK);
    const char* help1 = "USB-JTAG console is live:";
    const char* help2 = "  esptool write_flash 0xA0000 main.bin";
    const char* help3 = "  or REPL  >  reboot-ota";
    const char* help4 = "Factory reset: hold button A 5s at boot.";

    onebit::drawBitmapText(fb, bodyFont, 20, 155, help1, onebit::BLACK);
    onebit::drawBitmapText(fb, bodyFont, 20, 170, help2, onebit::BLACK);
    onebit::drawBitmapText(fb, bodyFont, 20, 185, help3, onebit::BLACK);
    onebit::drawBitmapText(fb, bodyFont, 20, 205, help4, onebit::BLACK);

    // Bottom-right heartbeat anchor will live at (396, 296) — see heartbeatTask.

    FramebufferAdapter adapter(fb);
    g_display->show(adapter);
}

void heartbeatTask(void* /*arg*/) {
    bool on = false;
    while (true) {
        if (g_fb && g_display) {
            // Toggle a single pixel at the bottom-right inside the border.
            // Full-screen refresh once a second is acceptable for recovery —
            // this isn't performance-critical.
            g_fb->setPixel(396, 296, on ? onebit::BLACK : onebit::WHITE);
            FramebufferAdapter adapter(*g_fb);
            g_display->show(adapter);
            on = !on;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

} // anonymous namespace

void startDisplay() {
    // DMA-capable allocator for the framebuffer — ST7305 does SPI DMA over
    // the raw buffer pointer so it must come from internal SRAM. Mirrors
    // esp32_rendering/main/main.cpp:294-300.
    static onebit::Allocator dma_allocator = {
        .alloc = [](size_t size, void*) -> void* {
            return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        },
        .free = [](void* ptr, void*) { heap_caps_free(ptr); },
        .ctx = nullptr,
    };
    onebit::init(dma_allocator);

    static onebit::Framebuffer<400, 300> fb;
    if (!fb.buffer()) {
        ESP_LOGE(TAG, "framebuffer alloc failed");
        return;
    }
    g_fb = &fb;

    static st7305::Config displayConfig;
    static st7305::Display display(displayConfig);
    display.init();
    g_display = &display;

    render(fb, esp_reset_reason());

    xTaskCreatePinnedToCore(
        heartbeatTask, "recovery_hb", 3072, nullptr,
        1,   // lowest priority
        nullptr,
        1    // Core 1 — keep Core 0 free for REPL
    );
}

} // namespace recovery
