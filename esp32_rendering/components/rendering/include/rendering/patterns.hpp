#pragma once

#include "framebuffer.hpp"
#include <cstdint>

namespace rendering {

/// Bayer 4x4 dithering matrix
extern const uint8_t BAYER_4X4[4][4];

/// 8x8 crosshatch pattern tile
extern const uint8_t CROSSHATCH_8X8[8];

/// Dither pattern levels
enum class Pattern : uint8_t {
    SolidBlack = 0,      ///< 100% ink
    Dense = 1,           ///< ~75% ink
    Medium = 2,          ///< ~50% ink
    Sparse = 3,          ///< ~25% ink
    SolidWhite = 4,      ///< 0% ink
    DenseCrosshatch = 5  ///< ~60% ink, diagonal crosshatch
};

/// Pattern thresholds for Bayer dithering
constexpr uint8_t PATTERN_THRESHOLDS[] = {16, 12, 8, 4, 0};

/// Test if pixel at (x,y) should be filled for given pattern
/// Inline for performance - called per-pixel
inline bool patternTest(Pattern pattern, int16_t x, int16_t y) {
    if (pattern == Pattern::SolidWhite) return false;
    if (pattern == Pattern::SolidBlack) return true;

    if (pattern == Pattern::DenseCrosshatch) {
        // Use 8x8 tile lookup (extern declared at namespace scope)
        uint8_t row = CROSSHATCH_8X8[y & 7];
        return row & (0x80 >> (x & 7));
    }

    // Bayer dithering for other patterns
    uint8_t threshold = PATTERN_THRESHOLDS[static_cast<uint8_t>(pattern)];
    uint8_t bayerValue = BAYER_4X4[y & 3][x & 3];
    return bayerValue < threshold;
}

/// Fill polygon with dither pattern
void fillPolygonPattern(IFramebuffer& fb, const Point* points, size_t count, Pattern pattern);

/// Fill rectangle with dither pattern
void fillRectPattern(IFramebuffer& fb, int16_t x, int16_t y, int16_t w, int16_t h, Pattern pattern);
void fillRectPattern(IFramebuffer& fb, const Rect& rect, Pattern pattern);

/// Fill circle with dither pattern
void fillCirclePattern(IFramebuffer& fb, int16_t cx, int16_t cy, int16_t r, Pattern pattern);
void fillCirclePattern(IFramebuffer& fb, Point center, int16_t r, Pattern pattern);

} // namespace rendering
