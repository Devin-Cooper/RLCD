#include "rendering/animation.hpp"
#include <cmath>

namespace rendering {

void wigglePoints(const PointF* points, size_t count, PointF* outPoints,
                  float amplitude, float frequency, float t, uint32_t seed) {
    // Match Python implementation using golden ratio-based phase offsets
    constexpr float PI2 = 2.0f * 3.14159265f;
    constexpr float PHI = 1.618f;      // Golden ratio
    constexpr float E = 2.718f;        // Euler's number
    constexpr float PI = 3.141f;       // Pi (for phase)
    constexpr float OFFSET_X = 2.399f; // Per-vertex X offset multiplier
    constexpr float OFFSET_Y = 3.141f; // Per-vertex Y offset multiplier

    for (size_t i = 0; i < count; i++) {
        // Unique phase offset per vertex based on index and seed
        float phaseX = seed * PHI + i * OFFSET_X;
        float phaseY = seed * E + i * OFFSET_Y;

        // Calculate displacement using sin/cos (matching Python)
        float dx = amplitude * std::sin(t * frequency * PI2 + phaseX);
        float dy = amplitude * std::cos(t * frequency * PI2 + phaseY);

        // Round to integers for pixel stability, then convert back to float
        outPoints[i].x = std::round(points[i].x + dx);
        outPoints[i].y = std::round(points[i].y + dy);
    }
}

void wigglePoints(const Point* points, size_t count, Point* outPoints,
                  float amplitude, float frequency, float t, uint32_t seed) {
    constexpr float PI2 = 2.0f * 3.14159265f;
    constexpr float PHI = 1.618f;
    constexpr float E = 2.718f;
    constexpr float OFFSET_X = 2.399f;
    constexpr float OFFSET_Y = 3.141f;

    for (size_t i = 0; i < count; i++) {
        float phaseX = seed * PHI + i * OFFSET_X;
        float phaseY = seed * E + i * OFFSET_Y;

        float dx = amplitude * std::sin(t * frequency * PI2 + phaseX);
        float dy = amplitude * std::cos(t * frequency * PI2 + phaseY);

        outPoints[i].x = static_cast<int16_t>(std::round(points[i].x + dx));
        outPoints[i].y = static_cast<int16_t>(std::round(points[i].y + dy));
    }
}

void transitionPoints(const PointF* pointsA, const PointF* pointsB, size_t count,
                      PointF* outPoints, float t, float (*easing)(float)) {
    // Apply easing if provided
    float easedT = easing ? easing(t) : t;
    easedT = clamp01(easedT);

    for (size_t i = 0; i < count; i++) {
        outPoints[i].x = lerp(pointsA[i].x, pointsB[i].x, easedT);
        outPoints[i].y = lerp(pointsA[i].y, pointsB[i].y, easedT);
    }
}

AnimationState::AnimationState(float startTime)
    : startTime_(startTime)
    , currentTime_(startTime) {
}

void AnimationState::update(float currentTime) {
    currentTime_ = currentTime;
}

void AnimationState::reset(float startTime) {
    startTime_ = startTime;
    currentTime_ = startTime;
}

void AnimationState::restart() {
    startTime_ = currentTime_;
}

} // namespace rendering
