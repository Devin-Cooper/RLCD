#include "rendering/clock_face.hpp"
#include "rendering/primitives.hpp"
#include "rendering/patterns.hpp"
#include "rendering/bezier.hpp"
#include "rendering/vector_font.hpp"
#include "rendering/shapes.hpp"
#include "rendering/animation.hpp"
#include <cmath>
#include <cstdio>

namespace rendering {

// Day abbreviations
static const char* DAY_ABBREVS[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

uint8_t to12Hour(uint8_t hours24) {
    uint8_t h = hours24 % 12;
    return h == 0 ? 12 : h;
}

const char* getDayAbbrev(uint8_t dayOfWeek) {
    if (dayOfWeek > 6) dayOfWeek = 0;
    return DAY_ABBREVS[dayOfWeek];
}

void renderObservatoryClock(IFramebuffer& fb, const ClockData& data,
                            const ClockAnimState& anim, uint32_t seed) {
    // Layout constants
    constexpr float MAIN_HEX_CX = 200.0f;
    constexpr float MAIN_HEX_CY = 120.0f;
    constexpr float MAIN_HEX_RADIUS = 110.0f;
    constexpr float MAIN_HEX_LUMPINESS = 0.05f;

    constexpr float SAT_HEX_RADIUS = 38.0f;
    constexpr float SAT_HEX_LUMPINESS = 0.08f;
    constexpr float SAT_HEX_Y = 255.0f;
    constexpr float SAT_HEX_X[] = {70.0f, 200.0f, 330.0f};

    // =========================================
    // Step 1: Clear to white
    // =========================================
    fb.clear(WHITE);

    // =========================================
    // Step 2: Fill with dense crosshatch background
    // =========================================
    fillRectPattern(fb, 0, 0, 400, 300, Pattern::DenseCrosshatch);

    // =========================================
    // Step 3: Main hex - white fill to punch through
    // =========================================
    PointF mainHexPts[6];
    generateHex(mainHexPts, 6, MAIN_HEX_CX, MAIN_HEX_CY, MAIN_HEX_RADIUS,
                MAIN_HEX_LUMPINESS, seed);

    // Apply wiggle animation to outline
    PointF wiggledMainHex[6];
    wigglePoints(mainHexPts, 6, wiggledMainHex, 1.0f, 0.5f, anim.elapsed, seed + 100);

    // Convert to int points for fill
    Point mainHexInt[6];
    for (int i = 0; i < 6; i++) {
        mainHexInt[i] = wiggledMainHex[i].toPoint();
    }
    fillPolygon(fb, mainHexInt, 6, WHITE);

    // =========================================
    // Step 4: Main hex outline - heavy splat brush
    // =========================================
    PointF mainBezierPts[7];  // 6 + 1 to close loop
    polygonToBezierLoop(wiggledMainHex, 6, mainBezierPts);
    strokeBezierTextureBall(fb, mainBezierPts, 7, BrushId::Heavy, 0.4f, 2.5f);

    // =========================================
    // Step 5: Time text (large, centered)
    // =========================================
    char timeStr[8];
    uint8_t displayHour = to12Hour(data.hours);
    if (anim.showColon) {
        snprintf(timeStr, sizeof(timeStr), "%2d:%02d", displayHour, data.minutes);
    } else {
        snprintf(timeStr, sizeof(timeStr), "%2d %02d", displayHour, data.minutes);
    }

    // Large time display (slightly smaller to fit)
    renderStringCentered(fb, timeStr, 200, 85, 38, 58, 5, 3, BLACK);

    // =========================================
    // Step 6: Satellite hexes with phase-offset breathing and drift
    // =========================================
    // Golden ratio phase offsets for organic staggered breathing
    constexpr float SAT_PHASES[3] = {0.0f, 0.382f, 0.618f};
    // Drift parameters - each hex drifts on its own slow path
    constexpr float DRIFT_RADIUS = 15.0f;  // Max drift distance
    constexpr float DRIFT_PERIOD_X[3] = {23.0f, 31.0f, 19.0f};  // Slow X periods (seconds)
    constexpr float DRIFT_PERIOD_Y[3] = {29.0f, 17.0f, 37.0f};  // Slow Y periods (seconds)
    constexpr float DRIFT_PHASE_X[3] = {0.0f, 2.1f, 4.2f};      // Phase offsets
    constexpr float DRIFT_PHASE_Y[3] = {1.5f, 3.7f, 0.8f};

    // Satellite data strings - date split into two lines
    char dayStr[8];
    snprintf(dayStr, sizeof(dayStr), "%s", getDayAbbrev(data.dayOfWeek));

    char dateNumStr[8];
    snprintf(dateNumStr, sizeof(dateNumStr), "%d/%d", data.month, data.day);

    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%d", data.tempF);  // Just number, degree symbol separate

    char humStr[8];
    snprintf(humStr, sizeof(humStr), "%d%%", data.humidity);

    for (int i = 0; i < 3; i++) {
        // Calculate drift offset for this hex
        float driftX = DRIFT_RADIUS * sinf((anim.elapsed + DRIFT_PHASE_X[i]) * 2.0f * 3.14159f / DRIFT_PERIOD_X[i]);
        float driftY = DRIFT_RADIUS * sinf((anim.elapsed + DRIFT_PHASE_Y[i]) * 2.0f * 3.14159f / DRIFT_PERIOD_Y[i]);

        float hexCX = SAT_HEX_X[i] + driftX;
        float hexCY = SAT_HEX_Y + driftY;

        // Generate satellite hex with per-satellite phase-offset breathing
        float breatheScale = breathingScaleWithPhase(
            anim.elapsed, 0.97f, 1.03f, 3.33f, SAT_PHASES[i]);
        float scaledRadius = SAT_HEX_RADIUS * breatheScale;
        PointF satHexPts[6];
        generateHex(satHexPts, 6, hexCX, hexCY, scaledRadius,
                    SAT_HEX_LUMPINESS, seed + 1000 + i);

        // Convert to int points
        Point satHexInt[6];
        for (int j = 0; j < 6; j++) {
            satHexInt[j] = satHexPts[j].toPoint();
        }

        // Fill black
        fillPolygon(fb, satHexInt, 6, BLACK);

        // Outline with fine brush
        PointF satBezierPts[7];
        polygonToBezierLoop(satHexPts, 6, satBezierPts);
        strokeBezierTextureBall(fb, satBezierPts, 7, BrushId::Fine, 0.4f, 2.0f);

        // White text with halo - date hex has two lines
        int16_t textCX = static_cast<int16_t>(hexCX);
        int16_t textCY = static_cast<int16_t>(hexCY);

        if (i == 0) {
            // Date hex: two lines - day name on top, date on bottom
            renderStringCenteredWithHalo(fb, dayStr, textCX, textCY - 10,
                                          12, 16, 2, 1, WHITE, BLACK);
            renderStringCenteredWithHalo(fb, dateNumStr, textCX, textCY + 10,
                                          12, 16, 2, 1, WHITE, BLACK);
        } else if (i == 1) {
            // Temperature hex
            renderStringCenteredWithHalo(fb, tempStr, textCX, textCY,
                                          12, 16, 2, 1, WHITE, BLACK);
        } else {
            // Humidity hex
            renderStringCenteredWithHalo(fb, humStr, textCX, textCY,
                                          12, 16, 2, 1, WHITE, BLACK);
        }
    }
}

} // namespace rendering
