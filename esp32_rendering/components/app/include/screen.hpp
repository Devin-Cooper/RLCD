// esp32_rendering/components/app/include/screen.hpp
#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "input_queue.hpp"

namespace app {

class ScreenStack;

/// Abstract base for all app screens.
///
/// Contract:
///  - An opaque Screen owns every pixel in fb when rendered.
///  - A transparent Screen must draw into a bounded region only and
///    MUST NOT call fb.clear().
///  - Screens MUST NOT call stack.push/pop/replace from render().
///    ScreenStack asserts a render-phase flag to catch this.
///  - Screens MUST NOT vTaskDelay for more than one frame (~33ms).
///  - The only mutation vector is handleInput(evt, stack). Async
///    callbacks (WiFi/BLE state) post synthetic input events; they
///    never touch the stack directly.
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
