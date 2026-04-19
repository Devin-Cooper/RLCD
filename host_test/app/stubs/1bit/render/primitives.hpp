#pragma once

#include "1bit/core/framebuffer.hpp"
#include "1bit/render/bitmap_font.hpp"
#include <cstdint>

namespace onebit {

// Stubs — menu.cpp's render() path calls these but tests never invoke render().
inline void fillRect(IFramebuffer&, int16_t, int16_t, int16_t, int16_t, Color) {}
inline void drawRect(IFramebuffer&, int16_t, int16_t, int16_t, int16_t, Color) {}
inline void drawBitmapText(IFramebuffer&, const BitmapFont&, int16_t, int16_t,
                           const char*, Color) {}

} // namespace onebit
