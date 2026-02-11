#pragma once

#include "types.hpp"
#include <cmath>
#include <cstdint>

namespace rendering {

/// Linear interpolation
inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/// Clamp value to range [min, max]
inline float clamp(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

/// Clamp t to [0, 1]
inline float clamp01(float t) {
    return clamp(t, 0.0f, 1.0f);
}

/// Cubic smoothstep: 3t² - 2t³
inline float easeInOut(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

/// Sine-based ease: (1 - cos(πt)) / 2
inline float easeInOutSine(float t) {
    t = clamp01(t);
    return (1.0f - std::cos(t * 3.14159265f)) * 0.5f;
}

/// Quadratic ease in
inline float easeIn(float t) {
    t = clamp01(t);
    return t * t;
}

/// Quadratic ease out
inline float easeOut(float t) {
    t = clamp01(t);
    return t * (2.0f - t);
}

/// Bounce effect at end
inline float easeOutBounce(float t) {
    t = clamp01(t);
    if (t < (1.0f / 2.75f)) {
        return 7.5625f * t * t;
    } else if (t < (2.0f / 2.75f)) {
        t -= (1.5f / 2.75f);
        return 7.5625f * t * t + 0.75f;
    } else if (t < (2.5f / 2.75f)) {
        t -= (2.25f / 2.75f);
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= (2.625f / 2.75f);
        return 7.5625f * t * t + 0.984375f;
    }
}

/// Breathing scale effect - oscillates between min and max scale
/// @param t Time in seconds
/// @param minScale Minimum scale factor
/// @param maxScale Maximum scale factor
/// @param period Period of one complete cycle in seconds
inline float breathingScale(float t, float minScale = 0.95f, float maxScale = 1.05f, float period = 3.0f) {
    float phase = std::fmod(t, period) / period;  // 0 to 1
    float sinValue = std::sin(phase * 2.0f * 3.14159265f);
    float normalized = (sinValue + 1.0f) * 0.5f;  // 0 to 1
    return minScale + normalized * (maxScale - minScale);
}

/// Breathing offset effect - oscillates position
/// @param t Time in seconds
/// @param amplitude Maximum offset in pixels
/// @param period Period of one complete cycle in seconds
inline float breathingOffset(float t, float amplitude = 2.0f, float period = 3.0f) {
    float phase = std::fmod(t, period) / period;
    return amplitude * std::sin(phase * 2.0f * 3.14159265f);
}

/// Simple hash function for deterministic per-vertex randomness
inline uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x85ebca6b;
    x ^= x >> 13;
    x *= 0xc2b2ae35;
    x ^= x >> 16;
    return x;
}

/// Deterministic per-vertex wiggle effect for PointF
/// @param points Input points
/// @param count Number of points
/// @param outPoints Output points (can be same as input)
/// @param amplitude Maximum displacement in pixels
/// @param frequency Oscillation speed
/// @param t Time in seconds
/// @param seed Random seed for determinism
void wigglePoints(const PointF* points, size_t count, PointF* outPoints,
                  float amplitude, float frequency, float t, uint32_t seed = 0);

/// Deterministic per-vertex wiggle effect for Point
void wigglePoints(const Point* points, size_t count, Point* outPoints,
                  float amplitude, float frequency, float t, uint32_t seed = 0);

/// Point transition (morph between two shapes)
/// @param pointsA Source shape
/// @param pointsB Target shape
/// @param count Number of points (must be same for both)
/// @param outPoints Output points
/// @param t Transition progress (0 = A, 1 = B)
/// @param easing Optional easing function
void transitionPoints(const PointF* pointsA, const PointF* pointsB, size_t count,
                      PointF* outPoints, float t, float (*easing)(float) = nullptr);

/// Animation state helper class
class AnimationState {
public:
    explicit AnimationState(float startTime = 0.0f);

    /// Update with current time
    void update(float currentTime);

    /// Get elapsed time since start
    float elapsed() const { return currentTime_ - startTime_; }

    /// Get current time
    float currentTime() const { return currentTime_; }

    /// Reset animation to new start time
    void reset(float startTime = 0.0f);

    /// Restart from current time
    void restart();

    /// Breathing scale convenience method
    float breathingScale(float minScale = 0.95f, float maxScale = 1.05f, float period = 3.0f) const {
        return rendering::breathingScale(elapsed(), minScale, maxScale, period);
    }

    /// Breathing offset convenience method
    float breathingOffset(float amplitude = 2.0f, float period = 3.0f) const {
        return rendering::breathingOffset(elapsed(), amplitude, period);
    }

    /// Transition progress for duration-based animations
    /// @param duration Duration of the transition
    /// @param delay Optional delay before starting
    float progress(float duration, float delay = 0.0f) const {
        float t = elapsed() - delay;
        if (t <= 0.0f) return 0.0f;
        if (t >= duration) return 1.0f;
        return t / duration;
    }

    /// Check if animation has completed
    bool isComplete(float duration, float delay = 0.0f) const {
        return elapsed() >= (duration + delay);
    }

private:
    float startTime_;
    float currentTime_;
};

} // namespace rendering
