#pragma once

#include <cstdint>
#include <cmath>

namespace rendering {

/// Integer point for pixel coordinates
struct Point {
    int16_t x, y;

    constexpr Point() : x(0), y(0) {}
    constexpr Point(int16_t x_, int16_t y_) : x(x_), y(y_) {}

    constexpr bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    constexpr bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    constexpr Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }

    constexpr Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }
};

/// Floating-point point for bezier curves and animation
struct PointF {
    float x, y;

    constexpr PointF() : x(0.0f), y(0.0f) {}
    constexpr PointF(float x_, float y_) : x(x_), y(y_) {}

    explicit PointF(const Point& p) : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)) {}

    Point toPoint() const {
        return Point(static_cast<int16_t>(std::round(x)),
                     static_cast<int16_t>(std::round(y)));
    }

    constexpr PointF operator+(const PointF& other) const {
        return PointF(x + other.x, y + other.y);
    }

    constexpr PointF operator-(const PointF& other) const {
        return PointF(x - other.x, y - other.y);
    }

    constexpr PointF operator*(float scalar) const {
        return PointF(x * scalar, y * scalar);
    }

    float length() const {
        return std::sqrt(x * x + y * y);
    }

    PointF normalized() const {
        float len = length();
        if (len < 0.0001f) return PointF(0.0f, 0.0f);
        return PointF(x / len, y / len);
    }
};

/// Rectangle with position and size
struct Rect {
    int16_t x, y, w, h;

    constexpr Rect() : x(0), y(0), w(0), h(0) {}
    constexpr Rect(int16_t x_, int16_t y_, int16_t w_, int16_t h_)
        : x(x_), y(y_), w(w_), h(h_) {}

    constexpr int16_t left() const { return x; }
    constexpr int16_t top() const { return y; }
    constexpr int16_t right() const { return x + w; }
    constexpr int16_t bottom() const { return y + h; }

    constexpr bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    constexpr bool contains(const Point& p) const {
        return contains(p.x, p.y);
    }
};

/// Color type - true = black (ink), false = white (paper)
using Color = bool;
constexpr Color BLACK = true;
constexpr Color WHITE = false;

} // namespace rendering
