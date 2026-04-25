// esp32_rendering/components/app/include/screen.hpp
#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "input_queue.hpp"
#include "span_view.hpp"

#include <cstdint>

namespace app {

class ScreenStack;
struct Command;  // forward, defined in command_registry.hpp

/// Coarse classification used by the Ctrl+K / Ctrl+/ input interception
/// and by the BleSoftToastWatcher's primary-view suppression rule.
enum class ScreenKind : uint8_t {
    Other,
    Dashboard,
    Terminal,
    Pairing,
    KeyboardGate,
    CommandPalette,
};

/// One row of the keybind footer / help cheat-sheet.
/// ASCII-only — arrow strings use bracket notation ("[up/dn]") rather
/// than UTF-8 glyphs, since the bitmap font's printable range is ASCII.
struct KeybindHint {
    char key[12];
    char label[16];
};

/// Abstract base for all app screens.
///
/// Screen contract
/// ---------------
/// Lifecycle:   onEnter() → (handleInput + render)* → onExit()
/// Rendering:   opaque screens own every pixel in fb;
///              transparent screens draw into a bounded region only and
///              MUST NOT call fb.clear().
/// Stack mut:   only via handleInput's stack arg. Render MUST NOT push/pop
///              (ScreenStack asserts via in_render_phase_).
/// Async:       background callbacks (WiFi/BLE state) post synthetic
///              InputEvents (Source::System + EventType::*StateChanged)
///              so Screens stay single-threaded.
/// Timing:      targetFps() paces the main loop; never vTaskDelay > 1 frame.
///
/// 2026-04-25 spec adds 7 defaulted virtuals for keyboard-first UX:
/// keybindHints / getContextualCommands / dispatchContextual /
/// wantsKeybindFooter / bypassesKeyboardGate / screenKind /
/// breadcrumbLabel. See per-virtual doc comments below.
class Screen {
public:
    virtual ~Screen() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual void handleInput(const input::InputEvent& evt,
                             ScreenStack& stack) = 0;
    virtual void render(onebit::IFramebuffer& fb,
                        const onebit::BitmapFont& font) = 0;

    virtual bool isTransparent() const { return false; }
    virtual int targetFps() const { return 10; }

    // --- New virtuals (2026-04-25 spec). All defaulted. ---

    /// Hints rendered into the bottom-of-screen 12 px keybind footer.
    /// Order is priority-ordered: rightmost hints get truncated when
    /// horizontal space runs out. The full list is shown in the help modal.
    virtual SpanView<const KeybindHint> keybindHints() const { return {}; }

    /// Per-screen "actions you can do here right now". Merged into the
    /// command palette before the global registry. May write into a
    /// screen-internal buffer on each call (non-const intentionally).
    virtual SpanView<const Command> getContextualCommands() { return {}; }

    /// Handles palette-driven contextual command ids (range 0xFF00..0xFFFE).
    /// Default no-op. Phase 12 wires this on each Screen that has contextual
    /// commands.
    virtual void dispatchContextual(uint16_t /*id*/) {}

    /// True (default) → 12 px footer drawn on this screen. Override false
    /// on Dashboard, Terminal, Pairing, KeyboardGate, and CommandPalette.
    virtual bool wantsKeybindFooter() const { return true; }

    /// True → ScreenStack's gate policy will not block a push of this
    /// Screen even if no BLE keyboard is bonded. Override true on
    /// Dashboard, Terminal, Pairing, KeyboardGate, CommandPalette.
    virtual bool bypassesKeyboardGate() const { return false; }

    /// Coarse classification used by input-interception guards and the
    /// soft-toast suppression rule. Default Other.
    virtual ScreenKind screenKind() const { return ScreenKind::Other; }

    /// Short label used in the help modal's breadcrumb line. ~12 chars max.
    virtual const char* breadcrumbLabel() const { return "Screen"; }
};

/// Walks the stack bottom-up calling `breadcrumbLabel()` on each. Joined with
/// " > ". If the joined string overflows `out_capacity`, the leftmost segments
/// are collapsed into "...". `out` is always NUL-terminated. Empty stack
/// produces an empty string.
void buildBreadcrumb(const ScreenStack& stack,
                     char* out, std::size_t out_capacity);

} // namespace app
