#pragma once

#include "framebuffer.hpp"
#include <cstdint>
#include <cstring>

namespace rendering {

/// Mask buffer for clipping operations
/// BLACK (true) = drawing allowed, WHITE (false) = drawing blocked
template<int16_t WIDTH = 400, int16_t HEIGHT = 300>
class MaskBuffer : public IFramebuffer {
public:
    static constexpr int16_t BYTES_PER_ROW = (WIDTH + 7) / 8;
    static constexpr size_t BUFFER_SIZE = static_cast<size_t>(BYTES_PER_ROW) * HEIGHT;

    MaskBuffer();
    ~MaskBuffer();

    // Non-copyable
    MaskBuffer(const MaskBuffer&) = delete;
    MaskBuffer& operator=(const MaskBuffer&) = delete;

    // Movable
    MaskBuffer(MaskBuffer&& other) noexcept;
    MaskBuffer& operator=(MaskBuffer&& other) noexcept;

    int16_t width() const override { return WIDTH; }
    int16_t height() const override { return HEIGHT; }

    void setPixel(int16_t x, int16_t y, Color color) override;
    Color getPixel(int16_t x, int16_t y) const override;
    void clear(Color color = WHITE) override;
    void fillSpan(int16_t y, int16_t xStart, int16_t xEnd, Color color) override;

    uint8_t* buffer() override { return buffer_; }
    const uint8_t* buffer() const override { return buffer_; }
    size_t bufferSize() const override { return BUFFER_SIZE; }

    /// Invert all pixels in the mask (for cutout effects)
    void invert();

private:
    uint8_t* buffer_;

    static constexpr uint8_t bitMask(int16_t x) {
        return 1 << (7 - (x & 7));
    }

    static constexpr size_t byteIndex(int16_t x, int16_t y) {
        return static_cast<size_t>(y) * BYTES_PER_ROW + (x >> 3);
    }
};

/// Default 400x300 mask buffer type
using MaskBuffer400x300 = MaskBuffer<400, 300>;

} // namespace rendering
