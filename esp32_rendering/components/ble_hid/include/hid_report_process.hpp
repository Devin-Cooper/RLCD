#pragma once

#include <cstdint>
#include <cstddef>
#include "hid_translate.hpp"

namespace ble_hid {

using Emit = void(*)(const KeyEvent& event, void* ctx);

/// Pure: process one HID boot-keyboard report body against caller-owned
/// prev_keys state. `body` is `[modifier, reserved, k1..k6]` (8 bytes) —
/// the caller is responsible for stripping any report-ID prefix before
/// calling this (per HOGP report-protocol format). Reports with
/// `body_len < 8` are ignored.
///
/// Updates `prev_keys` in place. Emits a callback for every newly-pressed
/// keycode (translated via translateKeycode).
void processHidReport(const uint8_t* body, std::size_t body_len,
                      uint8_t prev_keys[6],
                      Emit emit, void* ctx) noexcept;

} // namespace ble_hid
