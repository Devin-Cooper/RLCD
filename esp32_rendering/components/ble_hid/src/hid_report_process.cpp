#include "hid_report_process.hpp"
#include <cstring>

namespace ble_hid {

void processHidReport(const uint8_t* report, std::size_t len,
                      uint8_t prev_keys[6],
                      Emit emit, void* ctx) noexcept {
    if (len < 8) return;

    uint8_t modifiers = report[0];
    // report[1] is reserved

    for (int i = 2; i < 8; i++) {
        uint8_t key = report[i];
        if (key == 0) continue;

        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (prev_keys[j] == key) { was_pressed = true; break; }
        }
        if (!was_pressed) {
            KeyEvent event = translateKeycode(key, modifiers);
            if (event.length > 0 && emit) emit(event, ctx);
        }
    }

    std::memcpy(prev_keys, report + 2, 6);
}

} // namespace ble_hid
