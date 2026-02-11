#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace rendering {

// Forward declaration
template<int16_t WIDTH, int16_t HEIGHT>
class MaskBuffer;

/// Abstract interface for framebuffer operations
class IFramebuffer {
public:
    virtual ~IFramebuffer() = default;

    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;

    virtual void setPixel(int16_t x, int16_t y, Color color) = 0;
    virtual Color getPixel(int16_t x, int16_t y) const = 0;
    virtual void clear(Color color = WHITE) = 0;

    /// Set pixel without mask check (for internal use by pattern fills)
    virtual void setPixelDirect(int16_t x, int16_t y, Color color) {
        setPixel(x, y, color);  // Default: same as setPixel
    }

    /// Fast horizontal span fill
    /// @param xStart First pixel to fill (inclusive)
    /// @param xEnd One past last pixel to fill (exclusive, like Python range)
    virtual void fillSpan(int16_t y, int16_t xStart, int16_t xEnd, Color color) = 0;

    virtual uint8_t* buffer() = 0;
    virtual const uint8_t* buffer() const = 0;
    virtual size_t bufferSize() const = 0;

    /// Set mask for clipping (nullptr to disable)
    /// Default implementation does nothing (for buffers that don't support masking)
    virtual void setMask(IFramebuffer* /*mask*/) {}

    /// Get current mask (nullptr if none)
    virtual IFramebuffer* getMask() const { return nullptr; }

    /// Helper to set pixel using Point
    void setPixel(const Point& p, Color color) {
        setPixel(p.x, p.y, color);
    }

    /// Helper to get pixel using Point
    Color getPixel(const Point& p) const {
        return getPixel(p.x, p.y);
    }
};

/// Template implementation with compile-time sizing and PSRAM allocation
template<int16_t WIDTH = 400, int16_t HEIGHT = 300>
class Framebuffer : public IFramebuffer {
public:
    static constexpr int16_t BYTES_PER_ROW = (WIDTH + 7) / 8;
    static constexpr size_t BUFFER_SIZE = static_cast<size_t>(BYTES_PER_ROW) * HEIGHT;

    Framebuffer();
    ~Framebuffer();

    // Non-copyable
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    // Movable
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    /// Set mask for clipping (nullptr to disable)
    void setMask(IFramebuffer* mask) override { mask_ = mask; }

    /// Get current mask
    IFramebuffer* getMask() const override { return mask_; }

    int16_t width() const override { return WIDTH; }
    int16_t height() const override { return HEIGHT; }

    void setPixel(int16_t x, int16_t y, Color color) override;
    void setPixelDirect(int16_t x, int16_t y, Color color) override;
    Color getPixel(int16_t x, int16_t y) const override;
    void clear(Color color = WHITE) override;
    void fillSpan(int16_t y, int16_t xStart, int16_t xEnd, Color color) override;

    uint8_t* buffer() override { return buffer_; }
    const uint8_t* buffer() const override { return buffer_; }
    size_t bufferSize() const override { return BUFFER_SIZE; }

private:
    uint8_t* buffer_;
    IFramebuffer* mask_ = nullptr;

    static constexpr uint8_t bitMask(int16_t x) {
        return 1 << (7 - (x & 7));
    }

    static constexpr size_t byteIndex(int16_t x, int16_t y) {
        return static_cast<size_t>(y) * BYTES_PER_ROW + (x >> 3);
    }
};

/// Default 400x300 framebuffer type
using Framebuffer400x300 = Framebuffer<400, 300>;

} // namespace rendering
