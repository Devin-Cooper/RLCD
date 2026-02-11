#include "rendering/mask_buffer.hpp"
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char* TAG = "mask_buffer";

namespace rendering {

template<int16_t WIDTH, int16_t HEIGHT>
MaskBuffer<WIDTH, HEIGHT>::MaskBuffer() : buffer_(nullptr) {
    // Allocate in PSRAM for large buffers
    if (BUFFER_SIZE > 1024) {
        buffer_ = static_cast<uint8_t*>(
            heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!buffer_) {
            ESP_LOGW(TAG, "PSRAM allocation failed, falling back to SRAM");
            buffer_ = static_cast<uint8_t*>(
                heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_8BIT));
        }
    } else {
        buffer_ = static_cast<uint8_t*>(
            heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_8BIT));
    }

    if (!buffer_) {
        ESP_LOGE(TAG, "Failed to allocate mask buffer (%zu bytes)", BUFFER_SIZE);
    } else {
        ESP_LOGI(TAG, "Allocated %dx%d mask buffer (%zu bytes)", WIDTH, HEIGHT, BUFFER_SIZE);
        clear(WHITE);  // Default: nothing visible
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
MaskBuffer<WIDTH, HEIGHT>::~MaskBuffer() {
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
MaskBuffer<WIDTH, HEIGHT>::MaskBuffer(MaskBuffer&& other) noexcept
    : buffer_(other.buffer_) {
    other.buffer_ = nullptr;
}

template<int16_t WIDTH, int16_t HEIGHT>
MaskBuffer<WIDTH, HEIGHT>& MaskBuffer<WIDTH, HEIGHT>::operator=(MaskBuffer&& other) noexcept {
    if (this != &other) {
        if (buffer_) {
            heap_caps_free(buffer_);
        }
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
    }
    return *this;
}

template<int16_t WIDTH, int16_t HEIGHT>
void MaskBuffer<WIDTH, HEIGHT>::setPixel(int16_t x, int16_t y, Color color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || !buffer_) {
        return;
    }

    size_t idx = byteIndex(x, y);
    uint8_t mask = bitMask(x);

    if (color) {
        buffer_[idx] |= mask;
    } else {
        buffer_[idx] &= ~mask;
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
Color MaskBuffer<WIDTH, HEIGHT>::getPixel(int16_t x, int16_t y) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || !buffer_) {
        return WHITE;  // Outside bounds = blocked
    }

    size_t idx = byteIndex(x, y);
    uint8_t mask = bitMask(x);

    return (buffer_[idx] & mask) != 0;
}

template<int16_t WIDTH, int16_t HEIGHT>
void MaskBuffer<WIDTH, HEIGHT>::clear(Color color) {
    if (!buffer_) return;
    std::memset(buffer_, color ? 0xFF : 0x00, BUFFER_SIZE);
}

template<int16_t WIDTH, int16_t HEIGHT>
void MaskBuffer<WIDTH, HEIGHT>::fillSpan(int16_t y, int16_t xStart, int16_t xEnd, Color color) {
    if (!buffer_ || y < 0 || y >= HEIGHT) return;

    if (xStart < 0) xStart = 0;
    if (xEnd > WIDTH) xEnd = WIDTH;
    if (xStart >= xEnd) return;

    // Simple per-pixel implementation (mask doesn't need the optimized version)
    for (int16_t x = xStart; x < xEnd; ++x) {
        setPixel(x, y, color);
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
void MaskBuffer<WIDTH, HEIGHT>::invert() {
    if (!buffer_) return;
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        buffer_[i] = ~buffer_[i];
    }
}

// Explicit instantiation for common sizes
template class MaskBuffer<400, 300>;
template class MaskBuffer<200, 150>;
template class MaskBuffer<100, 75>;

} // namespace rendering
