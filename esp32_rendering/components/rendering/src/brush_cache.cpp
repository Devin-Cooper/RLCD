#include "rendering/brush_cache.hpp"
#include "rendering/primitives.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace rendering {

void BrushCache::init() {
    precomputeBrush(BrushId::Heavy, &DEFAULT_BALL_8X8[0][0], 8, 8);
    precomputeBrush(BrushId::Fine, &FINE_BRUSH_6X6[0][0], 6, 6);
    precomputeBrush(BrushId::Scratchy, &SCRATCHY_BRUSH_8X8[0][0], 8, 8);
    precomputeBrush(BrushId::Thin, &THIN_BRUSH_4X4[0][0], 4, 4);
    precomputeBrush(BrushId::Blobby, &BLOBBY_BRUSH_10X10[0][0], 10, 10);
}

void BrushCache::precomputeBrush(BrushId id, const bool* src, int srcWidth, int srcHeight) {
    int idx = static_cast<int>(id);
    BrushSet& set = sets_[idx];

    float halfW = srcWidth / 2.0f;
    float halfH = srcHeight / 2.0f;

    for (int r = 0; r < ROTATION_STEPS; r++) {
        float angle = (r * 2.0f * 3.14159265f) / ROTATION_STEPS;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        RotatedBrush& rb = set.rotations[r];
        std::memset(rb.pixels, 0, sizeof(rb.pixels));

        // Compute bounding box of rotated brush
        // Corners of source brush relative to center
        float corners[4][2] = {
            {-halfW, -halfH},
            { halfW, -halfH},
            {-halfW,  halfH},
            { halfW,  halfH}
        };

        float minX = 1000, maxX = -1000, minY = 1000, maxY = -1000;
        for (auto& c : corners) {
            float rx = c[0] * cosA - c[1] * sinA;
            float ry = c[0] * sinA + c[1] * cosA;
            minX = std::min(minX, rx);
            maxX = std::max(maxX, rx);
            minY = std::min(minY, ry);
            maxY = std::max(maxY, ry);
        }

        rb.offset_x = static_cast<int8_t>(std::floor(minX));
        rb.offset_y = static_cast<int8_t>(std::floor(minY));
        rb.width = static_cast<int8_t>(std::ceil(maxX) - std::floor(minX) + 1);
        rb.height = static_cast<int8_t>(std::ceil(maxY) - std::floor(minY) + 1);

        // Clamp to max stamp size
        if (rb.width > 14) rb.width = 14;
        if (rb.height > 14) rb.height = 14;

        // Rotate each source pixel and place into stamp
        for (int sy = 0; sy < srcHeight; sy++) {
            for (int sx = 0; sx < srcWidth; sx++) {
                if (!src[sy * srcWidth + sx]) continue;

                float dx = sx - halfW + 0.5f;
                float dy = sy - halfH + 0.5f;

                float rx = dx * cosA - dy * sinA;
                float ry = dx * sinA + dy * cosA;

                int px = static_cast<int>(std::round(rx)) - rb.offset_x;
                int py = static_cast<int>(std::round(ry)) - rb.offset_y;

                if (px >= 0 && px < rb.width && py >= 0 && py < rb.height) {
                    rb.pixels[py][px] = true;
                }
            }
        }
    }
}

int BrushCache::angleToIndex(float angle) {
    // Normalize angle to [0, 2π)
    constexpr float TWO_PI = 2.0f * 3.14159265f;
    float normalized = std::fmod(angle, TWO_PI);
    if (normalized < 0) normalized += TWO_PI;

    // Map to index [0, ROTATION_STEPS)
    int idx = static_cast<int>(std::round(normalized / TWO_PI * ROTATION_STEPS));
    return idx % ROTATION_STEPS;
}

const RotatedBrush& BrushCache::get(BrushId brush, float angle) const {
    int brushIdx = static_cast<int>(brush);
    int rotIdx = angleToIndex(angle);
    return sets_[brushIdx].rotations[rotIdx];
}

void stampRotatedBrush(IFramebuffer& fb, const RotatedBrush& brush,
                       float cx, float cy) {
    int baseX = static_cast<int>(std::round(cx)) + brush.offset_x;
    int baseY = static_cast<int>(std::round(cy)) + brush.offset_y;

    for (int py = 0; py < brush.height; py++) {
        for (int px = 0; px < brush.width; px++) {
            if (brush.pixels[py][px]) {
                fb.setPixel(static_cast<int16_t>(baseX + px),
                           static_cast<int16_t>(baseY + py), BLACK);
            }
        }
    }
}

void strokeBezierTextureBallCached(IFramebuffer& fb, const PointF* points, size_t count,
                                    BrushId brush, const BrushCache& cache,
                                    float smoothness, float spacing) {
    if (count < 2) return;

    // Generate tangent handles
    std::vector<TangentHandles> handles(count);
    autoTangent(points, count, handles.data(), smoothness);

    float distanceTraveled = 0.0f;
    float nextStampAt = 0.0f;
    bool firstStamp = true;

    for (size_t i = 0; i < count - 1; i++) {
        PointF p0 = points[i];
        PointF p1 = points[i + 1];
        PointF c0 = handles[i].out;
        PointF c1 = handles[i + 1].in;

        constexpr int STEPS_PER_SEGMENT = 50;
        PointF prevPt = p0;

        for (int step = 0; step <= STEPS_PER_SEGMENT; step++) {
            float t = static_cast<float>(step) / STEPS_PER_SEGMENT;
            PointF pt = cubicBezier(p0, c0, c1, p1, t);

            float stepDist = (pt - prevPt).length();
            distanceTraveled += stepDist;
            prevPt = pt;

            if (distanceTraveled >= nextStampAt || firstStamp) {
                PointF tangent = cubicBezierDerivative(p0, c0, c1, p1, t);
                float angle = std::atan2(tangent.y, tangent.x);

                const RotatedBrush& stamp = cache.get(brush, angle);
                stampRotatedBrush(fb, stamp, pt.x, pt.y);

                if (firstStamp) {
                    nextStampAt = spacing;
                    firstStamp = false;
                } else {
                    nextStampAt += spacing;
                }
            }
        }
    }
}

} // namespace rendering
