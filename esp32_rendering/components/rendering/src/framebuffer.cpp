#include "rendering/framebuffer.hpp"
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char* TAG = "framebuffer";

namespace rendering {

template<int16_t WIDTH, int16_t HEIGHT>
Framebuffer<WIDTH, HEIGHT>::Framebuffer() : buffer_(nullptr) {
    // Allocate in PSRAM for large buffers, SRAM for small
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
        ESP_LOGE(TAG, "Failed to allocate framebuffer (%zu bytes)", BUFFER_SIZE);
    } else {
        ESP_LOGI(TAG, "Allocated %dx%d framebuffer (%zu bytes)", WIDTH, HEIGHT, BUFFER_SIZE);
        clear(WHITE);
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
Framebuffer<WIDTH, HEIGHT>::~Framebuffer() {
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
Framebuffer<WIDTH, HEIGHT>::Framebuffer(Framebuffer&& other) noexcept
    : buffer_(other.buffer_) {
    other.buffer_ = nullptr;
}

template<int16_t WIDTH, int16_t HEIGHT>
Framebuffer<WIDTH, HEIGHT>& Framebuffer<WIDTH, HEIGHT>::operator=(Framebuffer&& other) noexcept {
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
void Framebuffer<WIDTH, HEIGHT>::setPixel(int16_t x, int16_t y, Color color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || !buffer_) {
        return;
    }

    size_t idx = byteIndex(x, y);
    uint8_t mask = bitMask(x);

    if (color) {
        buffer_[idx] |= mask;  // Set bit for black
    } else {
        buffer_[idx] &= ~mask; // Clear bit for white
    }
}

template<int16_t WIDTH, int16_t HEIGHT>
Color Framebuffer<WIDTH, HEIGHT>::getPixel(int16_t x, int16_t y) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || !buffer_) {
        return WHITE;
    }

    size_t idx = byteIndex(x, y);
    uint8_t mask = bitMask(x);

    return (buffer_[idx] & mask) != 0;
}

template<int16_t WIDTH, int16_t HEIGHT>
void Framebuffer<WIDTH, HEIGHT>::clear(Color color) {
    if (!buffer_) return;

    // BLACK fills with 0xFF (all bits set), WHITE fills with 0x00
    std::memset(buffer_, color ? 0xFF : 0x00, BUFFER_SIZE);
}

template<int16_t WIDTH, int16_t HEIGHT>
void Framebuffer<WIDTH, HEIGHT>::fillSpan(int16_t y, int16_t xStart, int16_t xEnd, Color color) {
    // xEnd is EXCLUSIVE (like Python) - fills from xStart to xEnd-1
    if (!buffer_ || y < 0 || y >= HEIGHT) return;

    // Clamp to valid range
    if (xStart < 0) xStart = 0;
    if (xEnd > WIDTH) xEnd = WIDTH;

    // Empty or invalid span
    if (xStart >= xEnd) return;

    // Calculate byte boundaries
    int16_t startByte = xStart >> 3;
    int16_t endByte = (xEnd - 1) >> 3;  // Byte containing the last pixel

    int16_t startBit = xStart & 7;
    int16_t endBit = (xEnd - 1) & 7;

    uint8_t* row = buffer_ + static_cast<size_t>(y) * BYTES_PER_ROW;

    if (startByte == endByte) {
        // All pixels are in the same byte
        uint8_t mask = 0;
        for (int16_t bit = startBit; bit <= endBit; ++bit) {
            mask |= (1 << (7 - bit));
        }
        if (color) {
            row[startByte] |= mask;
        } else {
            row[startByte] &= ~mask;
        }
    } else {
        // Span crosses multiple bytes
        int16_t firstFullByte = startByte;
        int16_t lastFullByte = endByte;

        // Handle partial start byte (if not byte-aligned)
        if (startBit != 0) {
            // Mask for bits from startBit to 7 (rightward to end of byte)
            uint8_t mask = (1 << (8 - startBit)) - 1;
            if (color) {
                row[startByte] |= mask;
            } else {
                row[startByte] &= ~mask;
            }
            firstFullByte = startByte + 1;
        }

        // Handle partial end byte (if not byte-aligned)
        if (endBit != 7) {
            // Mask for bits 0 to endBit (leftward from start of byte)
            uint8_t mask = static_cast<uint8_t>(0xFF << (7 - endBit)) & 0xFF;
            if (color) {
                row[endByte] |= mask;
            } else {
                row[endByte] &= ~mask;
            }
            lastFullByte = endByte - 1;
        }

        // Fill full bytes in the middle
        if (firstFullByte <= lastFullByte) {
            uint8_t fillValue = color ? 0xFF : 0x00;
            for (int16_t b = firstFullByte; b <= lastFullByte; ++b) {
                row[b] = fillValue;
            }
        }
    }
}

// Explicit instantiation for common sizes
template class Framebuffer<400, 300>;
template class Framebuffer<200, 150>;
template class Framebuffer<100, 75>;

} // namespace rendering
