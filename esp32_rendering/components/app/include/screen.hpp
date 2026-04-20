// esp32_rendering/components/app/include/screen.hpp
#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "input_queue.hpp"

namespace app {

class ScreenStack;

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
};

} // namespace app
