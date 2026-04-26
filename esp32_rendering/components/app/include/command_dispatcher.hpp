#pragma once
#include <cstdint>

namespace app {

struct ScreenContext;

/// Result of dispatchCommand. Tells the caller (typically the
/// CommandPalette) whether the dispatcher already mutated the screen
/// stack — so the palette knows whether to also self-pop.
///
/// Plan Amendment C: screens dispatched from the palette use
/// stack.replace() rather than push(), so the palette must NOT also pop
/// itself afterwards. Pure in-place actions (font cycle, reconnect, etc.)
/// return ScreenStays so the palette pops normally.
enum class DispatchResult : uint8_t {
    StackChanged,   // dispatcher pushed/replaced/cleared the stack — palette must NOT also self-pop
    ScreenStays,    // action ran in-place — palette should self-pop after
};

DispatchResult dispatchCommand(uint16_t id, ScreenContext& ctx);

} // namespace app
