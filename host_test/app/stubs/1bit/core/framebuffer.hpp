#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace onebit {

enum Color : uint8_t { BLACK = 0, WHITE = 1 };

class IFramebuffer {
public:
    virtual ~IFramebuffer() = default;
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;
    virtual void setPixel(int16_t x, int16_t y, Color c) = 0;
    virtual Color getPixel(int16_t x, int16_t y) const = 0;
    virtual void clear(Color c = WHITE) = 0;
    virtual void setPixelDirect(int16_t x, int16_t y, Color c) { setPixel(x, y, c); }
    virtual void fillSpan(int16_t y, int16_t x0, int16_t x1, Color c) {
        for (int16_t x = x0; x <= x1; ++x) setPixel(x, y, c);
    }
    virtual uint8_t* buffer() { return nullptr; }
    virtual const uint8_t* buffer() const { return nullptr; }
    virtual size_t bufferSize() const { return 0; }
};

template <int W, int H>
class Framebuffer : public IFramebuffer {
public:
    Framebuffer() : pixels_(W * H, WHITE) {}
    int16_t width()  const override { return W; }
    int16_t height() const override { return H; }
    void setPixel(int16_t x, int16_t y, Color c) override {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        pixels_[y * W + x] = c;
    }
    Color getPixel(int16_t x, int16_t y) const override {
        if (x < 0 || x >= W || y < 0 || y >= H) return WHITE;
        return pixels_[y * W + x];
    }
    void clear(Color c = WHITE) override {
        std::fill(pixels_.begin(), pixels_.end(), c);
    }
private:
    std::vector<Color> pixels_;
};

} // namespace onebit
