#pragma once

#include "framebuffer.hpp"
#include <cstdint>

namespace rendering {

/// A single stroke in a glyph (connected polyline)
struct GlyphStroke {
    const uint8_t* points;  ///< Packed x,y pairs (0-100 coordinate space)
    uint8_t pointCount;     ///< Number of points in the stroke
};

/// A complete glyph definition
struct Glyph {
    const GlyphStroke* strokes;
    uint8_t strokeCount;
};

/// Get glyph for character (nullptr if unsupported)
/// Supports 0-9, A-Z, and punctuation: : - . / ° %
const Glyph* getGlyph(char c);

/// Get character width multiplier (some characters are narrower)
/// Returns width as fraction of base width (e.g., 0.5 for half-width)
float getCharWidthMultiplier(char c);

/// Render single character
void renderChar(IFramebuffer& fb, char c, int16_t x, int16_t y,
                int16_t width, int16_t height, int16_t strokeWidth = 2, Color color = BLACK);

/// Render string (left-aligned)
void renderString(IFramebuffer& fb, const char* text, int16_t x, int16_t y,
                  int16_t charWidth, int16_t charHeight, int16_t spacing = 4,
                  int16_t strokeWidth = 2, Color color = BLACK);

/// Calculate string width (for alignment)
int16_t getStringWidth(const char* text, int16_t charWidth, int16_t spacing = 4);

/// Render string centered horizontally
void renderStringCentered(IFramebuffer& fb, const char* text, int16_t centerX, int16_t y,
                          int16_t charWidth, int16_t charHeight, int16_t spacing = 4,
                          int16_t strokeWidth = 2, Color color = BLACK);

/// Render string right-aligned
void renderStringRight(IFramebuffer& fb, const char* text, int16_t rightX, int16_t y,
                       int16_t charWidth, int16_t charHeight, int16_t spacing = 4,
                       int16_t strokeWidth = 2, Color color = BLACK);

/// Text alignment options for multiline rendering
enum class TextAlign { Left, Center, Right };

/// Render multiple lines of text
void renderMultiline(IFramebuffer& fb, const char* const* lines, size_t lineCount,
                     int16_t x, int16_t y, int16_t charWidth, int16_t charHeight,
                     int16_t lineSpacing = 8, TextAlign align = TextAlign::Left,
                     int16_t charSpacing = 4, int16_t strokeWidth = 2, Color color = BLACK);

} // namespace rendering
