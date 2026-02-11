#include "rendering/bezier.hpp"
#include "rendering/primitives.hpp"
#include <cmath>
#include <algorithm>

namespace rendering {

// Default 8x8 texture ball - organic scribble pattern
const bool DEFAULT_BALL_8X8[8][8] = {
    {false, false, true,  true,  true,  true,  false, false},
    {false, true,  true,  true,  true,  true,  true,  false},
    {true,  true,  false, true,  true,  false, true,  true },
    {true,  true,  true,  true,  true,  true,  true,  true },
    {true,  true,  true,  true,  true,  true,  true,  true },
    {true,  true,  false, true,  true,  false, true,  true },
    {false, true,  true,  true,  true,  true,  true,  false},
    {false, false, true,  true,  true,  true,  false, false}
};

// Fine 6x6 brush texture - smaller, tighter strokes
const bool FINE_BRUSH_6X6[6][6] = {
    {false, true,  true,  true,  true,  false},
    {true,  true,  true,  true,  true,  true },
    {true,  true,  false, false, true,  true },
    {true,  true,  false, false, true,  true },
    {true,  true,  true,  true,  true,  true },
    {false, true,  true,  true,  true,  false}
};

// Linear interpolation for floats
static inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Linear interpolation for points
static inline PointF lerpPoint(PointF a, PointF b, float t) {
    return PointF(lerp(a.x, b.x, t), lerp(a.y, b.y, t));
}

PointF cubicBezier(PointF p0, PointF p1, PointF p2, PointF p3, float t) {
    // De Casteljau's algorithm - three levels of linear interpolation
    PointF q0 = lerpPoint(p0, p1, t);
    PointF q1 = lerpPoint(p1, p2, t);
    PointF q2 = lerpPoint(p2, p3, t);

    PointF r0 = lerpPoint(q0, q1, t);
    PointF r1 = lerpPoint(q1, q2, t);

    return lerpPoint(r0, r1, t);
}

PointF cubicBezierDerivative(PointF p0, PointF p1, PointF p2, PointF p3, float t) {
    // Derivative control points: 3 * (P[i+1] - P[i])
    PointF d0 = (p1 - p0) * 3.0f;
    PointF d1 = (p2 - p1) * 3.0f;
    PointF d2 = (p3 - p2) * 3.0f;

    // Quadratic bezier on derivative control points
    PointF q0 = lerpPoint(d0, d1, t);
    PointF q1 = lerpPoint(d1, d2, t);

    return lerpPoint(q0, q1, t);
}

void autoTangent(const PointF* points, size_t count, TangentHandles* handles, float smoothness) {
    if (count < 2) return;

    for (size_t i = 0; i < count; i++) {
        PointF prev = (i == 0) ? points[0] : points[i - 1];
        PointF curr = points[i];
        PointF next = (i == count - 1) ? points[count - 1] : points[i + 1];

        // Tangent direction is parallel to prev->next
        PointF tangent = next - prev;
        float len = tangent.length();

        if (len < 0.0001f) {
            handles[i].in = curr;
            handles[i].out = curr;
            continue;
        }

        // Normalize and scale by distance to neighbors
        tangent = tangent * (1.0f / len);

        float distPrev = (curr - prev).length();
        float distNext = (next - curr).length();

        // Handle lengths based on smoothness and distance
        float handleLenIn = distPrev * smoothness * 0.5f;
        float handleLenOut = distNext * smoothness * 0.5f;

        handles[i].in = curr - tangent * handleLenIn;
        handles[i].out = curr + tangent * handleLenOut;
    }
}

// Calculate flatness (max distance from curve to baseline)
static float bezierFlatness(PointF p0, PointF c0, PointF c1, PointF p1) {
    // Calculate perpendicular distance of control points from baseline
    PointF baseline = p1 - p0;
    float baseLen = baseline.length();

    if (baseLen < 0.0001f) {
        // Degenerate case: check distance from p0
        float d0 = (c0 - p0).length();
        float d1 = (c1 - p0).length();
        return std::max(d0, d1);
    }

    // Cross product gives signed area, divide by length for distance
    float d0 = std::abs((c0.x - p0.x) * baseline.y - (c0.y - p0.y) * baseline.x) / baseLen;
    float d1 = std::abs((c1.x - p0.x) * baseline.y - (c1.y - p0.y) * baseline.x) / baseLen;

    return std::max(d0, d1);
}

void subdivideBezier(PointF p0, PointF c0, PointF c1, PointF p1,
                     std::vector<Point>& outPoints, float tolerance) {
    // Check flatness
    float flatness = bezierFlatness(p0, c0, c1, p1);

    if (flatness <= tolerance) {
        // Flat enough - add endpoint
        outPoints.push_back(p1.toPoint());
    } else {
        // Subdivide at t=0.5 using De Casteljau
        PointF q0 = lerpPoint(p0, c0, 0.5f);
        PointF q1 = lerpPoint(c0, c1, 0.5f);
        PointF q2 = lerpPoint(c1, p1, 0.5f);

        PointF r0 = lerpPoint(q0, q1, 0.5f);
        PointF r1 = lerpPoint(q1, q2, 0.5f);

        PointF mid = lerpPoint(r0, r1, 0.5f);

        // Recurse on both halves
        subdivideBezier(p0, q0, r0, mid, outPoints, tolerance);
        subdivideBezier(mid, r1, q2, p1, outPoints, tolerance);
    }
}

void drawBezierCurve(IFramebuffer& fb, const PointF* points, size_t count,
                     float smoothness, Color color, float tolerance) {
    if (count < 2) return;

    // Generate tangent handles
    std::vector<TangentHandles> handles(count);
    autoTangent(points, count, handles.data(), smoothness);

    // Generate polyline from all bezier segments
    std::vector<Point> polyline;
    polyline.push_back(points[0].toPoint());

    for (size_t i = 0; i < count - 1; i++) {
        PointF p0 = points[i];
        PointF p1 = points[i + 1];
        PointF c0 = handles[i].out;
        PointF c1 = handles[i + 1].in;

        subdivideBezier(p0, c0, c1, p1, polyline, tolerance);
    }

    // Draw polyline
    for (size_t i = 0; i + 1 < polyline.size(); i++) {
        drawLine(fb, polyline[i], polyline[i + 1], color);
    }
}

void stampTexture(IFramebuffer& fb, const bool* texture, int16_t texWidth, int16_t texHeight,
                  float cx, float cy, float angle) {
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    float halfW = texWidth / 2.0f;
    float halfH = texHeight / 2.0f;

    for (int16_t ty = 0; ty < texHeight; ty++) {
        for (int16_t tx = 0; tx < texWidth; tx++) {
            if (!texture[ty * texWidth + tx]) continue;

            // Offset from texture center
            float dx = tx - halfW + 0.5f;
            float dy = ty - halfH + 0.5f;

            // Rotate
            float rx = dx * cosA - dy * sinA;
            float ry = dx * sinA + dy * cosA;

            // Translate to world position
            int16_t px = static_cast<int16_t>(std::round(cx + rx));
            int16_t py = static_cast<int16_t>(std::round(cy + ry));

            fb.setPixel(px, py, BLACK);
        }
    }
}

void strokeBezierTextureBall(IFramebuffer& fb, const PointF* points, size_t count,
                              float smoothness, float spacing,
                              const bool* texture, int16_t texWidth, int16_t texHeight) {
    if (count < 2) return;

    // Use default texture if none provided
    if (!texture) {
        texture = &DEFAULT_BALL_8X8[0][0];
        texWidth = 8;
        texHeight = 8;
    }

    // Generate tangent handles
    std::vector<TangentHandles> handles(count);
    autoTangent(points, count, handles.data(), smoothness);

    // Track total distance traveled and next stamp distance
    float distanceTraveled = 0.0f;
    float nextStampAt = 0.0f;  // Stamp at first point immediately
    bool firstStamp = true;

    for (size_t i = 0; i < count - 1; i++) {
        PointF p0 = points[i];
        PointF p1 = points[i + 1];
        PointF c0 = handles[i].out;
        PointF c1 = handles[i + 1].in;

        // Walk segment with fine stepping
        constexpr int STEPS_PER_SEGMENT = 50;
        PointF prevPt = p0;

        for (int step = 0; step <= STEPS_PER_SEGMENT; step++) {
            float t = static_cast<float>(step) / STEPS_PER_SEGMENT;
            PointF pt = cubicBezier(p0, c0, c1, p1, t);

            // Calculate distance traveled in this step
            float stepDist = (pt - prevPt).length();
            distanceTraveled += stepDist;
            prevPt = pt;

            // Check if we should stamp at this location
            if (distanceTraveled >= nextStampAt || firstStamp) {
                PointF tangent = cubicBezierDerivative(p0, c0, c1, p1, t);
                float angle = std::atan2(tangent.y, tangent.x);

                stampTexture(fb, texture, texWidth, texHeight, pt.x, pt.y, angle);

                // Schedule next stamp
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

void strokeBezierTextureBall(IFramebuffer& fb, const PointF* points, size_t count,
                              BrushId brush, float smoothness, float spacing) {
    const bool* texture;
    int16_t texWidth, texHeight;

    switch (brush) {
        case BrushId::Fine:
            texture = &FINE_BRUSH_6X6[0][0];
            texWidth = 6;
            texHeight = 6;
            break;
        case BrushId::Heavy:
        default:
            texture = &DEFAULT_BALL_8X8[0][0];
            texWidth = 8;
            texHeight = 8;
            break;
    }

    strokeBezierTextureBall(fb, points, count, smoothness, spacing, texture, texWidth, texHeight);
}

} // namespace rendering
