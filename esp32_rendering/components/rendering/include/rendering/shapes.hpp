#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>

namespace rendering {

/// Fast hash-based noise for shape deformation
/// Returns value in range [-1, 1]
float hashNoise(int32_t index, uint32_t seed);

/// Generate hexagon vertices with organic lumpiness
/// @param outPoints Output array (must have space for 'count' points)
/// @param count Number of vertices (6 for hexagon, more for rounder shape)
/// @param cx, cy Center position
/// @param radius Base radius
/// @param lumpiness Deformation amount (0 = perfect, 0.1 = 10% variation)
/// @param seed Random seed for deterministic deformation
void generateHex(PointF* outPoints, size_t count,
                 float cx, float cy, float radius,
                 float lumpiness, uint32_t seed);

/// Copy polygon vertices for bezier stroke, closing the loop
/// @param polyPoints Input polygon vertices
/// @param polyCount Number of polygon vertices
/// @param outBezierPoints Output array (must have space for polyCount + 1 points)
void polygonToBezierLoop(const PointF* polyPoints, size_t polyCount,
                         PointF* outBezierPoints);

} // namespace rendering
