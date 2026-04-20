#pragma once

#include <1bit/render/bitmap_font.hpp>
#include <cstdint>

namespace app {

/// Returns the BitmapFont corresponding to the Settings::font_size index
/// (0 = 5x7, 1/default = 6x9, 2 = 8x12).
const onebit::BitmapFont& fontForSize(uint8_t size);

} // namespace app
