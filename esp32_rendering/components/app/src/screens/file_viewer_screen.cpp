#include "screens/file_viewer_screen.hpp"
#include "screen_stack.hpp"

namespace app {

// Stub — filled in by Task 15.
FileViewerScreen::FileViewerScreen(ScreenContext& ctx, std::string path)
    : ctx_(ctx), path_(std::move(path)) {
    breadcrumb_[0] = 0;
}

void FileViewerScreen::onEnter() {}
void FileViewerScreen::handleInput(const input::InputEvent& /*evt*/, ScreenStack& /*stack*/) {}
void FileViewerScreen::render(onebit::IFramebuffer& /*fb*/, const onebit::BitmapFont& /*font*/) {}
void FileViewerScreen::renderText(onebit::IFramebuffer&, const onebit::BitmapFont&) {}
void FileViewerScreen::renderHex(onebit::IFramebuffer&, const onebit::BitmapFont&) {}
void FileViewerScreen::renderTooLarge(onebit::IFramebuffer&, const onebit::BitmapFont&) {}
void FileViewerScreen::renderError(onebit::IFramebuffer&, const onebit::BitmapFont&) {}

SpanView<const KeybindHint> FileViewerScreen::keybindHints() const { return {}; }
SpanView<const Command> FileViewerScreen::getContextualCommands() { return {}; }
void FileViewerScreen::dispatchContextual(uint16_t /*id*/) {}

}  // namespace app
