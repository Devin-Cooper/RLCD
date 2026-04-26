#include "screens/server_list_screen.hpp"
#include "screens/server_edit_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "config_manager.hpp"
#include <1bit/render/primitives.hpp>
#include <array>
#include <cstring>
#include <cstdio>
#include <string>
#include <esp_timer.h>

namespace app {

namespace {
constexpr std::array<app::KeybindHint, 6> kHints = {{
    {"[up/dn]", "nav"},
    {"Enter",   "edit"},
    {"Sh-A",    "active"},
    {"Esc",     "back"},
    {"Sh-D",    "delete"},
    {"Ctrl+/",  "help"},
}};
static_assert(sizeof("[up/dn]") <= 12 && sizeof("active") <= 16,
              "kHints contains a string longer than KeybindHint capacity");
} // namespace

app::SpanView<const app::KeybindHint> ServerListScreen::keybindHints() const {
    return kHints;
}

ServerListScreen::ServerListScreen(ScreenContext& ctx) : ctx_(ctx) {}

int16_t ServerListScreen::computeRowY(int index) const {
    return list_start_y_ + static_cast<int16_t>(index) * row_h_;
}

void ServerListScreen::onSelectionChange(int old_index, int new_index) {
    if (old_index == new_index) return;
    if (!focus_y_initialized_) return;
    int16_t old_y = computeRowY(old_index);
    int16_t new_y = computeRowY(new_index);
    auto tag = makeTag(TweenKind::FocusRect, focus_id::ServerListScreen);
    ctx_.animator.start(tag, old_y, new_y, kFocusRectUs, esp_timer_get_time());
    prev_selected_y_ = new_y;
}

void ServerListScreen::onEnter() {
    sel_ = 0;
    focus_y_initialized_ = false;
    auto tag = makeTag(TweenKind::FocusRect, focus_id::ServerListScreen);
    ctx_.animator.cancel(tag);
    // Spec Decision 10: surface one Toast per affected server per boot for
    // any server that loaded with use_key_auth=true but empty ssh_key_id.
    // ConfigManager owns the list; a local Set prevents re-showing when the
    // user navigates away and back within the same boot. The list is cleared
    // only by ConfigManager::markRepicked() on picker success (Phase 13).
    // Copy by value — markRepicked (called from Phase 13's picker callback)
    // would invalidate iterators into the ConfigManager's vector otherwise.
    auto indices = ctx_.configMgr.needsRepickIndices();
    for (int idx : indices) {
        if (idx < 0 || idx >= ctx_.configMgr.serverCount()) continue;
        const char* name = ctx_.configMgr.getServer(idx).creds.name;
        if (!shown_repick_names_.insert(name).second) continue;
        char msg[96];
        std::snprintf(msg, sizeof(msg), "Re-select SSH key for '%s'", name);
        ctx_.overlay.showToast(msg, 3000);
    }
}

int ServerListScreen::rowCount() const {
    return ctx_.configMgr.serverCount() + 1;
}

void ServerListScreen::openEditorForSelection(ScreenStack& stack) {
    int count = ctx_.configMgr.serverCount();
    if (sel_ == count) {
        stack.push(std::make_unique<ServerEditScreen>(ctx_, -1));
    } else {
        stack.push(std::make_unique<ServerEditScreen>(ctx_, sel_));
    }
}

void ServerListScreen::handleInput(const input::InputEvent& evt,
                                   ScreenStack& stack) {
    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        int count = rowCount();
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            int old_sel = sel_;
            if (evt.data[2] == 'A') sel_ = (sel_ - 1 + count) % count;
            if (evt.data[2] == 'B') sel_ = (sel_ + 1) % count;
            if (evt.data[2] == 'A' || evt.data[2] == 'B')
                onSelectionChange(old_sel, sel_);
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == '\r') {
            openEditorForSelection(stack);
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            stack.pop();
            return;
        }
        // Shift+A set active + reconnect (Amendment K)
        if (evt.data_length == 1 && evt.data[0] == 'A' &&
            sel_ < ctx_.configMgr.serverCount()) {
            ctx_.configMgr.setActiveServer(sel_);
            if (ctx_.switchToActiveServer) ctx_.switchToActiveServer();
            char msg[64];
            snprintf(msg, sizeof(msg), "Active: %s",
                     ctx_.configMgr.getServer(sel_).creds.name);
            ctx_.overlay.showToast(msg, 2000);
            return;
        }
        // Shift+D delete with confirm
        if (evt.data_length == 1 && evt.data[0] == 'D' &&
            sel_ < ctx_.configMgr.serverCount()) {
            int idx = sel_;
            std::string name_str(ctx_.configMgr.getServer(idx).creds.name);

            char body[64];
            snprintf(body, sizeof(body), "Delete %s?", name_str.c_str());
            ctx_.overlay.showConfirm("Confirm", body,
                [this, idx, name_str](bool yes) {
                    if (!yes) return;
                    if (ctx_.configMgr.deleteServer(idx)) {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Deleted %s",
                                 name_str.c_str());
                        ctx_.overlay.showToast(msg, 2000);
                        int rc = ctx_.configMgr.serverCount() + 1;
                        if (sel_ >= rc) sel_ = rc - 1;
                        // Row count changed; cancel tween and re-init y.
                        auto tag = makeTag(TweenKind::FocusRect,
                                           focus_id::ServerListScreen);
                        ctx_.animator.cancel(tag);
                        focus_y_initialized_ = false;
                    } else {
                        ctx_.overlay.showError("Delete failed",
                                                "NVS write error");
                    }
                });
            return;
        }
    }

    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonShort) {
        int count = rowCount();
        if (evt.button_id == 0) { stack.pop(); return; }
        if (evt.button_id == 1) {
            int old_sel = sel_;
            sel_ = (sel_ + 1) % count;
            onSelectionChange(old_sel, sel_);
        }
    }
    if (evt.source == input::Source::Button &&
        evt.type == input::EventType::ButtonLong &&
        evt.button_id == 1) {
        openEditorForSelection(stack);
    }
}

void ServerListScreen::render(onebit::IFramebuffer& fb,
                              const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8, "Servers", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2, fb.width() - 20, 1,
                     onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 8;

    // Cache list layout for the focus-rect animation.
    list_start_y_ = y - 1;
    row_h_        = font.glyph_height + 2;

    // Compute the focus-rect y. Animate if a tween is in progress.
    auto tag = makeTag(TweenKind::FocusRect, focus_id::ServerListScreen);
    int64_t now = esp_timer_get_time();
    if (!focus_y_initialized_) {
        prev_selected_y_ = computeRowY(sel_);
        focus_y_initialized_ = true;
    }
    int16_t cur_y = ctx_.animator.inProgress(tag, now)
                  ? ctx_.animator.value(tag, now)
                  : prev_selected_y_;

    // Draw the focus rect once at the (possibly interpolated) y.
    onebit::fillRect(fb, 8, cur_y, fb.width() - 16,
                     font.glyph_height + 2, onebit::BLACK);

    int count = ctx_.configMgr.serverCount();
    int active = ctx_.configMgr.activeServerIndex();
    for (int i = 0; i < count &&
         y + font.glyph_height < fb.height() - 20; ++i) {
        const auto& c = ctx_.configMgr.getServer(i).creds;
        char line[160];   // name(32) + user(32) + host(64) + port(5) + fixed chars
        snprintf(line, sizeof(line), "%c %c %s (%s@%s:%d)",
                 i == sel_ ? '>' : ' ',
                 i == active ? '*' : ' ',
                 c.name, c.username, c.host, c.port);
        onebit::drawBitmapText(fb, font, 10, y, line,
                               i == sel_ ? onebit::WHITE : onebit::BLACK);
        y += font.glyph_height + 2;
    }

    // Add-new row
    const char* add = "> [+ Add new...]";
    const char* add_dim = "  [+ Add new...]";
    bool add_sel = (sel_ == count);
    onebit::drawBitmapText(fb, font, 10, y, add_sel ? add : add_dim,
                           add_sel ? onebit::WHITE : onebit::BLACK);

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Up/Dn  Enter edit  Shift+A active  Shift+D delete  Esc back",
        onebit::BLACK);
}

} // namespace app
