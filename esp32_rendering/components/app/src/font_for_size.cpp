#include "font_for_size.hpp"
#include <1bit/fonts/term_5x7.hpp>
#include <1bit/fonts/term_6x9.hpp>
#include <1bit/fonts/term_8x12.hpp>

namespace app {

const onebit::BitmapFont& fontForSize(uint8_t size) {
    switch (size) {
        case 0: return onebit::fonts::TERM_5X7;
        case 2: return onebit::fonts::TERM_8X12;
        default: return onebit::fonts::TERM_6X9;
    }
}

} // namespace app
