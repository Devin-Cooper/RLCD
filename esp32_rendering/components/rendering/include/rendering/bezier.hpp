#pragma once

#include "framebuffer.hpp"
#include <vector>

namespace rendering {

/// Tangent handles for bezier control points
struct TangentHandles {
    PointF in;
    PointF out;
};

/// Cubic bezier evaluation using De Casteljau's algorithm
PointF cubicBezier(PointF p0, PointF p1, PointF p2, PointF p3, float t);

/// Cubic bezier derivative (tangent) at parameter t
PointF cubicBezierDerivative(PointF p0, PointF p1, PointF p2, PointF p3, float t);

/// Auto-generate smooth tangent handles (Catmull-Rom style)
void autoTangent(const PointF* points, size_t count, TangentHandles* handles, float smoothness = 0.5f);

/// Adaptive subdivision to polyline based on flatness tolerance
void subdivideBezier(PointF p0, PointF c0, PointF c1, PointF p1,
                     std::vector<Point>& outPoints, float tolerance = 1.0f);

/// Draw bezier curve as connected line segments
void drawBezierCurve(IFramebuffer& fb, const PointF* points, size_t count,
                     float smoothness = 0.5f, Color color = BLACK, float tolerance = 1.0f);

/// Default 8x8 texture for strokes (organic scribble pattern)
extern const bool DEFAULT_BALL_8X8[8][8];

/// Stamp texture at position with rotation
void stampTexture(IFramebuffer& fb, const bool* texture, int16_t texWidth, int16_t texHeight,
                  float cx, float cy, float angle);

/// Texture-ball stroke along bezier curve (Pope's technique)
void strokeBezierTextureBall(IFramebuffer& fb, const PointF* points, size_t count,
                              float smoothness = 0.5f, float spacing = 2.0f,
                              const bool* texture = nullptr, int16_t texWidth = 8, int16_t texHeight = 8);

/// Brush texture selection for strokes
enum class BrushId {
    Heavy = 0,    ///< Default 8x8 organic brush
    Fine = 1,     ///< Smaller 6x6 brush for delicate lines
    Scratchy = 2, ///< 8x8 rough edges
    Thin = 3,     ///< 4x4 hairline
    Blobby = 4    ///< 10x10 chunky
};

/// Fine 6x6 brush texture for satellite hex outlines
extern const bool FINE_BRUSH_6X6[6][6];

/// Scratchy 8x8 brush texture - rough, uneven edges
extern const bool SCRATCHY_BRUSH_8X8[8][8];

/// Thin 4x4 brush texture - hairline strokes
extern const bool THIN_BRUSH_4X4[4][4];

/// Blobby 10x10 brush texture - chunky, blobby strokes
extern const bool BLOBBY_BRUSH_10X10[10][10];

/// Texture-ball stroke with brush selection
void strokeBezierTextureBall(IFramebuffer& fb, const PointF* points, size_t count,
                              BrushId brush, float smoothness = 0.5f, float spacing = 2.0f);

} // namespace rendering
