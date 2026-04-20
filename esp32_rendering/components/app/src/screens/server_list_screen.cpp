#include "screens/server_list_screen.hpp"
#include "screens/server_edit_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "config_manager.hpp"
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <cstdio>
#include <string>

namespace app {

ServerListScreen::ServerListScreen(ScreenContext& ctx) : ctx_(ctx) {}

void ServerListScreen::onEnter() {
    sel_ = 0;
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
            if (evt.data[2] == 'A') sel_ = (sel_ - 1 + count) % count;
            if (evt.data[2] == 'B') sel_ = (sel_ + 1) % count;
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
        if (evt.button_id == 1) sel_ = (sel_ + 1) % count;
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
        if (i == sel_) {
            onebit::fillRect(fb, 8, y - 1, fb.width() - 16,
                             font.glyph_height + 2, onebit::BLACK);
            onebit::drawBitmapText(fb, font, 10, y, line, onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
        }
        y += font.glyph_height + 2;
    }

    // Add-new row
    const char* add = "> [+ Add new...]";
    const char* add_dim = "  [+ Add new...]";
    bool add_sel = (sel_ == count);
    if (add_sel) {
        onebit::fillRect(fb, 8, y - 1, fb.width() - 16,
                         font.glyph_height + 2, onebit::BLACK);
        onebit::drawBitmapText(fb, font, 10, y, add, onebit::WHITE);
    } else {
        onebit::drawBitmapText(fb, font, 10, y, add_dim, onebit::BLACK);
    }

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Up/Dn  Enter edit  Shift+A active  Shift+D delete  Esc back",
        onebit::BLACK);
}

} // namespace app
