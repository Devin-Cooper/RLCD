#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "animator.hpp"
#include "command_ids.hpp"
#include "command_registry.hpp"

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

    app::SpanView<const app::KeybindHint> keybindHints() const override;

    app::SpanView<const app::Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;

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

    // Phase 5: focus-rect animation
    int16_t prev_selected_y_ = 0;
    bool    focus_y_initialized_ = false;
    int16_t list_start_y_ = 0;
    int16_t row_h_ = 0;
    int16_t computeRowY(int index) const;
    void    onSelectionChange(int old_index, int new_index);
};

} // namespace app
