#pragma once

#include "screen.hpp"
#include "screen_context.hpp"

#include <functional>
#include <string>

namespace app {

/// SSH key management list. Two modes:
///   Browse — opened from MenuScreen; offers New/Import/Rename/Delete
///            hotkeys and Enter → SshKeyDetailScreen.
///   Picker — opened from ServerEditScreen Key row; top row is a synthetic
///            "(password auth)" entry, Enter fires `on_pick` with the
///            selected 32-char hex id (empty for password auth) and pops.
///
/// Picker pops itself BEFORE invoking `on_pick` so the callback may push
/// its own follow-up screen without getting eaten by the deferred pop.
class SshKeyListScreen : public Screen {
public:
    enum class Mode { Browse, Picker };

    explicit SshKeyListScreen(ScreenContext& ctx);
    SshKeyListScreen(ScreenContext& ctx,
                     std::function<void(const std::string& id_hex)> on_pick);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    ScreenContext& ctx_;
    Mode mode_;
    std::function<void(const std::string&)> on_pick_;
    int sel_ = 0;

    int rowCount() const;
    void openDetail(ScreenStack& stack);
    void beginRename(ScreenStack& stack);
    void confirmDelete();
    void firePickAndPop(ScreenStack& stack);
};

} // namespace app
