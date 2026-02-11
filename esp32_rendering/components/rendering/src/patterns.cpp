#include "rendering/patterns.hpp"
#include <algorithm>
#include <vector>

namespace rendering {

// Bayer 4x4 dithering matrix (standard pattern)
const uint8_t BAYER_4X4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

// 8x8 dense crosshatch pattern (~60% ink coverage)
// Diagonal lines at 45° and 135° with 2px thickness, superimposed
const uint8_t CROSSHATCH_8X8[8] = {
    0b11000011,  // row 0: 4 bits
    0b11100111,  // row 1: 6 bits
    0b01111110,  // row 2: 6 bits
    0b00111100,  // row 3: 4 bits
    0b00111100,  // row 4: 4 bits
    0b01111110,  // row 5: 6 bits
    0b11100111,  // row 6: 6 bits
    0b11000011   // row 7: 4 bits  (total: 40 bits = 62.5%)
};

void fillPolygonPattern(IFramebuffer& fb, const Point* points, size_t count, Pattern pattern) {
    if (count < 3) return;

    // Solid patterns use optimized fill
    if (pattern == Pattern::SolidBlack) {
        // Use scanline fill algorithm with solid color
        // Find bounding box
        int16_t minY = points[0].y;
        int16_t maxY = points[0].y;
        for (size_t i = 1; i < count; i++) {
            minY = std::min(minY, points[i].y);
            maxY = std::max(maxY, points[i].y);
        }

        minY = std::max<int16_t>(minY, 0);
        maxY = std::min<int16_t>(maxY, fb.height() - 1);

        std::vector<int16_t> intersections;
        intersections.reserve(count);

        for (int16_t y = minY; y <= maxY; y++) {
            intersections.clear();

            for (size_t i = 0; i < count; i++) {
                size_t next = (i + 1) % count;
                int16_t y0 = points[i].y;
                int16_t y1 = points[next].y;

                if (y0 == y1) continue;

                int16_t x0 = points[i].x;
                int16_t x1 = points[next].x;
                if (y0 > y1) {
                    std::swap(y0, y1);
                    std::swap(x0, x1);
                }

                if (y >= y0 && y < y1) {
                    int16_t x = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                    intersections.push_back(x);
                }
            }

            std::sort(intersections.begin(), intersections.end());

            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                fb.fillSpan(y, intersections[i], intersections[i + 1], BLACK);
            }
        }
        return;
    }

    if (pattern == Pattern::SolidWhite) {
        return;  // Nothing to draw
    }

    // Dithered pattern - need per-pixel test
    int16_t minY = points[0].y;
    int16_t maxY = points[0].y;
    for (size_t i = 1; i < count; i++) {
        minY = std::min(minY, points[i].y);
        maxY = std::max(maxY, points[i].y);
    }

    minY = std::max<int16_t>(minY, 0);
    maxY = std::min<int16_t>(maxY, fb.height() - 1);

    std::vector<int16_t> intersections;
    intersections.reserve(count);

    for (int16_t y = minY; y <= maxY; y++) {
        intersections.clear();

        for (size_t i = 0; i < count; i++) {
            size_t next = (i + 1) % count;
            int16_t y0 = points[i].y;
            int16_t y1 = points[next].y;

            if (y0 == y1) continue;

            int16_t x0 = points[i].x;
            int16_t x1 = points[next].x;
            if (y0 > y1) {
                std::swap(y0, y1);
                std::swap(x0, x1);
            }

            if (y >= y0 && y < y1) {
                int16_t x = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                intersections.push_back(x);
            }
        }

        std::sort(intersections.begin(), intersections.end());

        // Fill with pattern test (xEnd from intersections is already exclusive)
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int16_t xStart = std::max<int16_t>(intersections[i], 0);
            int16_t xEnd = std::min<int16_t>(intersections[i + 1], fb.width());
            for (int16_t x = xStart; x < xEnd; x++) {
                if (patternTest(pattern, x, y)) {
                    fb.setPixel(x, y, BLACK);
                }
            }
        }
    }
}

void fillRectPattern(IFramebuffer& fb, int16_t x, int16_t y, int16_t w, int16_t h, Pattern pattern) {
    if (pattern == Pattern::SolidWhite) return;

    int16_t x0 = std::max<int16_t>(x, 0);
    int16_t y0 = std::max<int16_t>(y, 0);
    int16_t x1 = std::min<int16_t>(x + w, fb.width());   // Exclusive end
    int16_t y1 = std::min<int16_t>(y + h, fb.height()); // Exclusive end

    if (pattern == Pattern::SolidBlack) {
        for (int16_t row = y0; row < y1; row++) {
            fb.fillSpan(row, x0, x1, BLACK);
        }
        return;
    }

    for (int16_t row = y0; row < y1; row++) {
        for (int16_t col = x0; col < x1; col++) {
            if (patternTest(pattern, col, row)) {
                fb.setPixel(col, row, BLACK);
            }
        }
    }
}

void fillRectPattern(IFramebuffer& fb, const Rect& rect, Pattern pattern) {
    fillRectPattern(fb, rect.x, rect.y, rect.w, rect.h, pattern);
}

void fillCirclePattern(IFramebuffer& fb, int16_t cx, int16_t cy, int16_t r, Pattern pattern) {
    if (r <= 0 || pattern == Pattern::SolidWhite) return;

    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;

    // Lambda takes exclusive xEnd
    auto fillSpanPattern = [&](int16_t spanY, int16_t xStart, int16_t xEnd) {
        if (pattern == Pattern::SolidBlack) {
            fb.fillSpan(spanY, xStart, xEnd, BLACK);
        } else {
            xStart = std::max<int16_t>(xStart, 0);
            xEnd = std::min<int16_t>(xEnd, fb.width());
            for (int16_t px = xStart; px < xEnd; px++) {
                if (patternTest(pattern, px, spanY)) {
                    fb.setPixel(px, spanY, BLACK);
                }
            }
        }
    };

    while (x <= y) {
        // Pass exclusive end (add 1 to inclusive endpoint)
        fillSpanPattern(cy + y, cx - x, cx + x + 1);
        fillSpanPattern(cy - y, cx - x, cx + x + 1);
        fillSpanPattern(cy + x, cx - y, cx + y + 1);
        fillSpanPattern(cy - x, cx - y, cx + y + 1);

        if (d <= 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void fillCirclePattern(IFramebuffer& fb, Point center, int16_t r, Pattern pattern) {
    fillCirclePattern(fb, center.x, center.y, r, pattern);
}

} // namespace rendering
