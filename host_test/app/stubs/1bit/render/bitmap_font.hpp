#pragma once

#include <cstdint>

namespace onebit {

struct BitmapFont {
    int16_t glyph_width  = 5;
    int16_t glyph_height = 7;
    const uint8_t* glyph_data = nullptr;
};

} // namespace onebit
