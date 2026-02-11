#pragma once

#include "framebuffer.hpp"

namespace rendering {

/// Draw a line using Bresenham's algorithm
void drawLine(IFramebuffer& fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color);
void drawLine(IFramebuffer& fb, Point p0, Point p1, Color color);

/// Draw a thick line with specified width
void drawThickLine(IFramebuffer& fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t width, Color color);
void drawThickLine(IFramebuffer& fb, Point p0, Point p1, int16_t width, Color color);

/// Draw polygon outline
void drawPolygon(IFramebuffer& fb, const Point* points, size_t count, Color color);

/// Fill polygon using scanline algorithm (even-odd rule)
void fillPolygon(IFramebuffer& fb, const Point* points, size_t count, Color color);

/// Draw rectangle outline
void drawRect(IFramebuffer& fb, int16_t x, int16_t y, int16_t w, int16_t h, Color color);
void drawRect(IFramebuffer& fb, const Rect& rect, Color color);

/// Fill rectangle
void fillRect(IFramebuffer& fb, int16_t x, int16_t y, int16_t w, int16_t h, Color color);
void fillRect(IFramebuffer& fb, const Rect& rect, Color color);

/// Draw circle outline using midpoint algorithm
void drawCircle(IFramebuffer& fb, int16_t cx, int16_t cy, int16_t r, Color color);
void drawCircle(IFramebuffer& fb, Point center, int16_t r, Color color);

/// Fill circle using horizontal spans
void fillCircle(IFramebuffer& fb, int16_t cx, int16_t cy, int16_t r, Color color);
void fillCircle(IFramebuffer& fb, Point center, int16_t r, Color color);

} // namespace rendering
