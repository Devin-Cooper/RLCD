#include "rendering/primitives.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace rendering {

// Bresenham's line algorithm
void drawLine(IFramebuffer& fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color) {
    int16_t dx = std::abs(x1 - x0);
    int16_t dy = std::abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (true) {
        fb.setPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void drawLine(IFramebuffer& fb, Point p0, Point p1, Color color) {
    drawLine(fb, p0.x, p0.y, p1.x, p1.y, color);
}

void drawThickLine(IFramebuffer& fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t width, Color color) {
    if (width <= 1) {
        drawLine(fb, x0, y0, x1, y1, color);
        return;
    }

    // Calculate perpendicular direction
    float dx = static_cast<float>(x1 - x0);
    float dy = static_cast<float>(y1 - y0);
    float len = std::sqrt(dx * dx + dy * dy);

    if (len < 0.0001f) {
        // Degenerate case: draw a filled circle at the point
        fillCircle(fb, x0, y0, width / 2, color);
        return;
    }

    // Normalized perpendicular
    float px = -dy / len;
    float py = dx / len;

    // Draw parallel lines for thickness
    float halfWidth = (width - 1) / 2.0f;
    for (float offset = -halfWidth; offset <= halfWidth; offset += 1.0f) {
        int16_t ox = static_cast<int16_t>(std::round(offset * px));
        int16_t oy = static_cast<int16_t>(std::round(offset * py));
        drawLine(fb, x0 + ox, y0 + oy, x1 + ox, y1 + oy, color);
    }
}

void drawThickLine(IFramebuffer& fb, Point p0, Point p1, int16_t width, Color color) {
    drawThickLine(fb, p0.x, p0.y, p1.x, p1.y, width, color);
}

void drawPolygon(IFramebuffer& fb, const Point* points, size_t count, Color color) {
    if (count < 2) return;

    for (size_t i = 0; i < count; i++) {
        size_t next = (i + 1) % count;
        drawLine(fb, points[i], points[next], color);
    }
}

void fillPolygon(IFramebuffer& fb, const Point* points, size_t count, Color color) {
    if (count < 3) return;

    // Find bounding box
    int16_t minY = points[0].y;
    int16_t maxY = points[0].y;
    for (size_t i = 1; i < count; i++) {
        minY = std::min(minY, points[i].y);
        maxY = std::max(maxY, points[i].y);
    }

    // Clamp to framebuffer
    minY = std::max<int16_t>(minY, 0);
    maxY = std::min<int16_t>(maxY, fb.height() - 1);

    // Scanline fill
    std::vector<int16_t> intersections;
    intersections.reserve(count);

    for (int16_t y = minY; y <= maxY; y++) {
        intersections.clear();

        // Find intersections with all edges
        for (size_t i = 0; i < count; i++) {
            size_t next = (i + 1) % count;
            int16_t y0 = points[i].y;
            int16_t y1 = points[next].y;

            // Skip horizontal edges
            if (y0 == y1) continue;

            // Ensure y0 < y1
            int16_t x0 = points[i].x;
            int16_t x1 = points[next].x;
            if (y0 > y1) {
                std::swap(y0, y1);
                std::swap(x0, x1);
            }

            // Check if scanline intersects this edge (exclude top vertex for even-odd)
            if (y >= y0 && y < y1) {
                // Integer intersection calculation
                int16_t x = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                intersections.push_back(x);
            }
        }

        // Sort intersections
        std::sort(intersections.begin(), intersections.end());

        // Fill between pairs (even-odd rule)
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            fb.fillSpan(y, intersections[i], intersections[i + 1], color);
        }
    }
}

void drawRect(IFramebuffer& fb, int16_t x, int16_t y, int16_t w, int16_t h, Color color) {
    // Top and bottom
    for (int16_t i = x; i < x + w; i++) {
        fb.setPixel(i, y, color);
        fb.setPixel(i, y + h - 1, color);
    }
    // Left and right
    for (int16_t j = y; j < y + h; j++) {
        fb.setPixel(x, j, color);
        fb.setPixel(x + w - 1, j, color);
    }
}

void drawRect(IFramebuffer& fb, const Rect& rect, Color color) {
    drawRect(fb, rect.x, rect.y, rect.w, rect.h, color);
}

void fillRect(IFramebuffer& fb, int16_t x, int16_t y, int16_t w, int16_t h, Color color) {
    int16_t x0 = std::max<int16_t>(x, 0);
    int16_t y0 = std::max<int16_t>(y, 0);
    int16_t x1 = std::min<int16_t>(x + w, fb.width());  // Exclusive end
    int16_t y1 = std::min<int16_t>(y + h, fb.height()); // Exclusive end

    for (int16_t row = y0; row < y1; row++) {
        fb.fillSpan(row, x0, x1, color);
    }
}

void fillRect(IFramebuffer& fb, const Rect& rect, Color color) {
    fillRect(fb, rect.x, rect.y, rect.w, rect.h, color);
}

// Midpoint circle algorithm
void drawCircle(IFramebuffer& fb, int16_t cx, int16_t cy, int16_t r, Color color) {
    if (r <= 0) return;

    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;

    while (x <= y) {
        // Draw 8 octant-symmetric points
        fb.setPixel(cx + x, cy + y, color);
        fb.setPixel(cx - x, cy + y, color);
        fb.setPixel(cx + x, cy - y, color);
        fb.setPixel(cx - x, cy - y, color);
        fb.setPixel(cx + y, cy + x, color);
        fb.setPixel(cx - y, cy + x, color);
        fb.setPixel(cx + y, cy - x, color);
        fb.setPixel(cx - y, cy - x, color);

        if (d <= 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void drawCircle(IFramebuffer& fb, Point center, int16_t r, Color color) {
    drawCircle(fb, center.x, center.y, r, color);
}

void fillCircle(IFramebuffer& fb, int16_t cx, int16_t cy, int16_t r, Color color) {
    if (r <= 0) return;

    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;

    while (x <= y) {
        // Fill horizontal spans for all 4 quadrants (xEnd is exclusive)
        fb.fillSpan(cy + y, cx - x, cx + x + 1, color);
        fb.fillSpan(cy - y, cx - x, cx + x + 1, color);
        fb.fillSpan(cy + x, cx - y, cx + y + 1, color);
        fb.fillSpan(cy - x, cx - y, cx + y + 1, color);

        if (d <= 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void fillCircle(IFramebuffer& fb, Point center, int16_t r, Color color) {
    fillCircle(fb, center.x, center.y, r, color);
}

} // namespace rendering
