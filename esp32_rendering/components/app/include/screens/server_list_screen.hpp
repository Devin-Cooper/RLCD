#pragma once
#include "screen.hpp"
#include "screen_context.hpp"
#include "animator.hpp"
#include "command_ids.hpp"
#include "command_registry.hpp"
#include <set>
#include <string>

namespace app {

class ServerListScreen : public Screen {
public:
    explicit ServerListScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt,
                     ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb,
                const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

    app::SpanView<const app::Command> getContextualCommands() override;
    void dispatchContextual(uint16_t id) override;

private:
    ScreenContext& ctx_;
    int sel_ = 0;     // 0..count-1 = servers; count = "[+ Add new...]"
    // Spec Decision 10: one-Toast-per-affected-server-per-boot tracking. The
    // ConfigManager-owned needs_repick_ list remains populated (cleared only
    // by markRepicked() on picker success); this set suppresses duplicate
    // Toasts within the current boot if the user revisits the screen.
    std::set<std::string> shown_repick_names_;
    int rowCount() const;
    void openEditorForSelection(ScreenStack& stack);
    // Phase 12 contextual helpers — also called by handleInput
    // shortcuts so the keypress path and the palette path share logic.
    void setHighlightedActive();
    void editHighlighted();
    void deleteHighlighted();
    void addServer();

    // Phase 5: focus-rect animation
    int16_t prev_selected_y_ = 0;
    bool    focus_y_initialized_ = false;
    int16_t list_start_y_ = 0;
    int16_t row_h_ = 0;
    int16_t computeRowY(int index) const;
    void    onSelectionChange(int old_index, int new_index);
};

} // namespace app
