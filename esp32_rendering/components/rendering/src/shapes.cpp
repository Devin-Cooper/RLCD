#include "rendering/shapes.hpp"
#include <cmath>

namespace rendering {

float hashNoise(int32_t index, uint32_t seed) {
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(index) * 374761393u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (static_cast<float>(h & 0xFFFF) / 32768.0f) - 1.0f;
}

void generateHex(PointF* outPoints, size_t count,
                 float cx, float cy, float radius,
                 float lumpiness, uint32_t seed) {
    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 2.0f * PI;

    for (size_t i = 0; i < count; i++) {
        float angle = (TWO_PI * static_cast<float>(i)) / static_cast<float>(count);
        angle -= PI / 2.0f;  // Start from top

        float noise = hashNoise(static_cast<int32_t>(i), seed);
        float r = radius * (1.0f + noise * lumpiness);

        outPoints[i].x = cx + r * std::cos(angle);
        outPoints[i].y = cy + r * std::sin(angle);
    }
}

void polygonToBezierLoop(const PointF* polyPoints, size_t polyCount,
                         PointF* outBezierPoints) {
    // Copy points and close the loop for bezier stroke
    // The strokeBezierTextureBall function handles tangent generation via autoTangent()
    for (size_t i = 0; i < polyCount; i++) {
        outBezierPoints[i] = polyPoints[i];
    }
    outBezierPoints[polyCount] = polyPoints[0];  // Close loop
}

} // namespace rendering
