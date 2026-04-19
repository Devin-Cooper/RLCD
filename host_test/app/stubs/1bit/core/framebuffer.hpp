#pragma once

#include <cstdint>
#include <cstddef>

namespace onebit {

enum Color : uint8_t { WHITE = 0, BLACK = 1 };

/// Abstract framebuffer interface — stub for host tests that never draw.
class IFramebuffer {
public:
    virtual ~IFramebuffer() = default;
    virtual int16_t width()  const = 0;
    virtual int16_t height() const = 0;
    virtual void setPixel(int16_t x, int16_t y, Color c) = 0;
    virtual Color getPixel(int16_t x, int16_t y) const = 0;
};

} // namespace onebit
