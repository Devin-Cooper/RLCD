#pragma once

#include <cstdint>
#include <cstddef>
#include "hid_translate.hpp"

namespace ble_hid {

using Emit = void(*)(const KeyEvent& event, void* ctx);

/// Pure: process one 8-byte HID boot-keyboard report against caller-owned
/// prev_keys state. Updates prev_keys in place. Emits callbacks for each
/// newly-pressed key. Reports with len < 8 are ignored.
/// Layout: [modifier, reserved, k1, k2, k3, k4, k5, k6].
void processHidReport(const uint8_t* report, std::size_t len,
                      uint8_t prev_keys[6],
                      Emit emit, void* ctx) noexcept;

} // namespace ble_hid
