#pragma once

#include "bezier.hpp"
#include <cstdint>

namespace rendering {

/// Number of pre-computed rotation steps (22.5 degree increments)
constexpr int ROTATION_STEPS = 16;

/// Number of brush types
constexpr int BRUSH_COUNT = 5;

/// A single pre-rotated brush stamp
struct RotatedBrush {
    bool pixels[14][14];  // Max rotated size (10x10 diagonal ≈ 14)
    int8_t width;
    int8_t height;
    int8_t offset_x;      // Offset from center to top-left of stamp
    int8_t offset_y;
};

/// All rotations for one brush
struct BrushSet {
    RotatedBrush rotations[ROTATION_STEPS];
};

/// Pre-computed rotated brush stamps for fast rendering
class BrushCache {
public:
    /// Pre-compute all rotations for all brushes
    void init();

    /// Get the closest pre-rotated stamp for a given brush and angle
    /// @param brush Brush type
    /// @param angle Angle in radians
    const RotatedBrush& get(BrushId brush, float angle) const;

private:
    BrushSet sets_[BRUSH_COUNT];

    /// Pre-compute rotations for a specific brush
    void precomputeBrush(BrushId id, const bool* src, int srcWidth, int srcHeight);

    /// Map angle (radians) to rotation index
    static int angleToIndex(float angle);
};

/// Stamp a pre-rotated brush at a position
void stampRotatedBrush(IFramebuffer& fb, const RotatedBrush& brush,
                       float cx, float cy);

/// Texture-ball stroke using BrushCache for fast stamping
void strokeBezierTextureBallCached(IFramebuffer& fb, const PointF* points, size_t count,
                                    BrushId brush, const BrushCache& cache,
                                    float smoothness = 0.5f, float spacing = 2.0f);

} // namespace rendering
