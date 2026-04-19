#pragma once

#include <cstdint>

namespace ble_hid {

/// Terminal-ready key event: already converted from HID to byte sequence.
struct KeyEvent {
    uint8_t bytes[8];   // Terminal byte sequence (e.g., "\x1b[A" for arrow up)
    uint8_t length;     // Number of valid bytes
};

// HID modifier bit masks (standard HID 1.11).
static constexpr uint8_t MOD_LCTRL  = 0x01;
static constexpr uint8_t MOD_LSHIFT = 0x02;
static constexpr uint8_t MOD_LALT   = 0x04;
static constexpr uint8_t MOD_LGUI   = 0x08;
static constexpr uint8_t MOD_RCTRL  = 0x10;
static constexpr uint8_t MOD_RSHIFT = 0x20;
static constexpr uint8_t MOD_RALT   = 0x40;
static constexpr uint8_t MOD_RGUI   = 0x80;

/// Pure: translate a HID keycode + modifier byte to a terminal byte sequence.
/// Returns a KeyEvent with length==0 for unmapped keys.
KeyEvent translateKeycode(uint8_t keycode, uint8_t modifiers) noexcept;

} // namespace ble_hid
