#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "dir_listing.hpp"

#include <functional>
#include <string>
#include <vector>

namespace app {

class FileBrowserScreen : public Screen {
public:
    explicit FileBrowserScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    SpanView<const KeybindHint> keybindHints() const override;
    SpanView<const Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;
    const char* breadcrumbLabel() const override { return "Files"; }

private:
    enum class State { OK, NoSdcard };

    ScreenContext& ctx_;
    fb::DirListing listing_;
    std::string current_path_ = "/sdcard";
    int selected_ = 0;
    int scroll_top_ = 0;
    fb::DirListing::HiddenMode hidden_ = fb::DirListing::HiddenMode::Hide;
    std::vector<std::string> nav_history_;
    std::function<void()> last_failed_op_;

    State state_ = State::OK;
    int err_focus_ = 0;  // 0 = Retry, 1 = Back

    void reload();
    bool ensureMounted();
    void enterSelected(ScreenStack& stack);
    void goUp(ScreenStack& stack);
    int  visibleRows(int screen_h) const;
    void drawRow(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                 int y, const fb::FileEntry& e, bool selected) const;

    void runDelete(const std::string& full_path, bool is_dir);
    void runRename(const std::string& old_full, const std::string& new_name);
    void runMkdir(const std::string& parent, const std::string& new_name);
    void showMidOpError(const char* op, int err_code);

    static const char* validateName(const std::string& name);
    static void formatSize(uint64_t bytes, char buf[16]);
    static void formatMtime(time_t mt, char buf[16]);
};

}  // namespace app
