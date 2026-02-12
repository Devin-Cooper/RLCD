#pragma once

#include "framebuffer.hpp"
#include "types.hpp"
#include <cstdint>

namespace rendering {

/// Clock display data
struct ClockData {
    uint8_t hours;      // 0-23 (will display as 12-hour)
    uint8_t minutes;
    uint8_t dayOfWeek;  // 0=Sun, 1=Mon, ..., 6=Sat
    uint8_t month;      // 1-12
    uint8_t day;        // 1-31
    int8_t tempF;       // Temperature in Fahrenheit
    uint8_t humidity;   // 0-100%
    uint8_t battery;    // 0-100% battery level
};

/// Clock animation state
struct ClockAnimState {
    float elapsed;      // Time in seconds
    bool showColon;     // Colon visibility for blinking
};

/// Render the Observatory clock face
/// @param fb Framebuffer to render to
/// @param data Clock data to display
/// @param anim Animation state
/// @param seed Random seed for shape generation (use consistent value)
void renderObservatoryClock(IFramebuffer& fb, const ClockData& data,
                            const ClockAnimState& anim, uint32_t seed);

/// Format hours to 12-hour display (returns 1-12)
uint8_t to12Hour(uint8_t hours24);

/// Get day-of-week abbreviation (SUN, MON, TUE, etc.)
const char* getDayAbbrev(uint8_t dayOfWeek);

} // namespace rendering
