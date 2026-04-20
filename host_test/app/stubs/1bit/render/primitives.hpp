#pragma once
#include "1bit/core/framebuffer.hpp"
#include "1bit/render/bitmap_font.hpp"
#include <cstring>

namespace onebit {

// Variadic no-op stubs that silently accept any argument count so
// existing source files (menu.cpp, dashboard.cpp) compile unchanged
// regardless of whether they pass a scale/color argument or not.

inline void fillRect(IFramebuffer&, int16_t, int16_t, int16_t, int16_t, Color) {}
inline void drawRect(IFramebuffer&, int16_t, int16_t, int16_t, int16_t, Color) {}
inline void drawLine(IFramebuffer&, int16_t, int16_t, int16_t, int16_t, Color) {}

// drawBitmapText: 6-arg form (menu.cpp) and 7-arg form (dashboard.cpp)
inline void drawBitmapText(IFramebuffer&, const BitmapFont&, int16_t, int16_t, const char*, Color) {}
inline void drawBitmapText(IFramebuffer&, const BitmapFont&, int16_t, int16_t, const char*, Color, int) {}

// getBitmapTextWidth: 2-arg form (plan stub) and 3-arg form (dashboard.cpp)
inline int16_t getBitmapTextWidth(const BitmapFont&, const char* s) {
    return s ? static_cast<int16_t>(std::strlen(s) * 8) : 0;
}
inline int16_t getBitmapTextWidth(const BitmapFont& f, const char* s, int /*scale*/) {
    return getBitmapTextWidth(f, s);
}

} // namespace onebit
