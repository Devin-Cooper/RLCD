#include "screens/server_edit_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "config_manager.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_types.hpp"
#include <1bit/render/primitives.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace app {

ServerEditScreen::ServerEditScreen(ScreenContext& ctx, int index)
    : ctx_(ctx), index_(index),
      ti_name_(name_, sizeof(name_), TextInputOpts{}),
      ti_host_(host_, sizeof(host_), TextInputOpts{}),
      ti_port_(port_, sizeof(port_), TextInputOpts{.numeric = true}),
      ti_user_(user_, sizeof(user_), TextInputOpts{}),
      ti_pw_(pw_, sizeof(pw_), TextInputOpts{
          .masked = true, .tab_toggles_reveal = false}) {}

void ServerEditScreen::onEnter() {
    if (index_ >= 0 && index_ < ctx_.configMgr.serverCount()) {
        const auto& c = ctx_.configMgr.getServer(index_).creds;
        std::strncpy(name_, c.name, sizeof(name_) - 1);
        std::strncpy(host_, c.host, sizeof(host_) - 1);
        std::snprintf(port_, sizeof(port_), "%u", c.port);
        std::strncpy(user_, c.username, sizeof(user_) - 1);
        std::strncpy(pw_,   c.password, sizeof(pw_)   - 1);
    }
}

TextInput& ServerEditScreen::activeField() {
    switch (focus_) {
        case 0: return ti_name_;
        case 1: return ti_host_;
        case 2: return ti_port_;
        case 3: return ti_user_;
        case 4: return ti_pw_;
        default: return ti_name_;
    }
}

void ServerEditScreen::saveAndPop(ScreenStack& stack) {
    sdcard::ServerCreds c{};
    std::strncpy(c.name, name_, sizeof(c.name) - 1);
    std::strncpy(c.host, host_, sizeof(c.host) - 1);
    c.port = static_cast<uint16_t>(std::atoi(port_));
    if (c.port == 0) c.port = 22;
    std::strncpy(c.username, user_, sizeof(c.username) - 1);
    std::strncpy(c.password, pw_, sizeof(c.password) - 1);
    // Preserve existing ssh_key_id + use_key_auth when editing an existing
    // server — the picker is the only thing that can change them. A prior
    // version unconditionally reset use_key_auth=false, which silently
    // downgraded every edit of a key-auth server back to password-auth.
    if (index_ >= 0 && index_ < ctx_.configMgr.serverCount()) {
        const auto& existing = ctx_.configMgr.getServer(index_).creds;
        c.use_key_auth = existing.use_key_auth;
        std::strncpy(c.ssh_key_id, existing.ssh_key_id,
                     sizeof(c.ssh_key_id) - 1);
    } else {
        c.use_key_auth = false;
        c.ssh_key_id[0] = '\0';
    }

    int written = ctx_.configMgr.upsertServer(c, index_ < 0 ? -1 : index_);
    if (written < 0) {
        ctx_.overlay.showError("Save failed",
            index_ < 0 ? "Server limit reached (max 8)" : "NVS write error");
        return;
    }
    bool editing_active = (written == ctx_.configMgr.activeServerIndex());
    char msg[96];
    if (editing_active) {
        snprintf(msg, sizeof(msg),
                 "Saved: %s (Shift+A to reconnect)", c.name);
    } else {
        snprintf(msg, sizeof(msg), "Saved: %s", c.name);
    }
    ctx_.overlay.showToast(msg, 2500);
    stack.pop();
}

void ServerEditScreen::cancelWithConfirm(ScreenStack& stack) {
    (void)stack;  // present for consistency with handleInput signature; callback uses ctx_.stack
    if (!dirty_) { ctx_.stack.pop(); return; }
    ctx_.overlay.showConfirm("Confirm", "Discard changes?",
        [this](bool yes) {
            if (yes) ctx_.stack.pop();
        });
}

void ServerEditScreen::handleInput(const input::InputEvent& evt,
                                   ScreenStack& stack) {
    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) return;

    // Focus slots: 0..4 = text fields, 5 = Key row, 6 = Save, 7 = Cancel.
    constexpr int kFocusCount = 8;

    if (evt.data_length == 1 && evt.data[0] == '\t') {
        focus_ = (focus_ + 1) % kFocusCount; return;
    }
    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        if (evt.data[2] == 'A') focus_ = (focus_ - 1 + kFocusCount) % kFocusCount;
        if (evt.data[2] == 'B') focus_ = (focus_ + 1) % kFocusCount;
        return;
    }

    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        cancelWithConfirm(stack); return;
    }

    if (focus_ == 5 && evt.data_length == 1 && evt.data[0] == '\r') {
        // Phase 13 swaps this stub for a push of SshKeyListScreen in Picker
        // mode with a callback that writes ssh_key_id + use_key_auth back
        // to the current server via ConfigManager + calls markRepicked().
        ctx_.overlay.showToast("Key picker (Phase 13)", 1500);
        return;
    }
    if (focus_ == 6 && evt.data_length == 1 && evt.data[0] == '\r') {
        saveAndPop(stack); return;
    }
    if (focus_ == 7 && evt.data_length == 1 && evt.data[0] == '\r') {
        cancelWithConfirm(stack); return;
    }

    // Only forward keystrokes to the TextInput layer when focus is on a
    // field (0..4). The Key/Save/Cancel rows don't have a TextInput and
    // shouldn't mark the screen dirty on random keypresses.
    if (focus_ >= 0 && focus_ <= 4) {
        TextInput& ti = activeField();
        TextInputResult r = ti.handleKey(evt.data, evt.data_length);
        if (r == TextInputResult::Submit) {
            focus_ = (focus_ + 1) % kFocusCount;
        }
        dirty_ = true;
    }
}

void ServerEditScreen::render(onebit::IFramebuffer& fb,
                              const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 6,
        index_ < 0 ? "New Server" : "Edit Server", onebit::BLACK);

    struct Row { const char* label; TextInput* ti; };
    Row rows[] = {
        {"Name:     ", &ti_name_},
        {"Host:     ", &ti_host_},
        {"Port:     ", &ti_port_},
        {"User:     ", &ti_user_},
        {"Password: ", &ti_pw_},
    };

    int16_t y = 30;
    for (int i = 0; i < 5; ++i) {
        char marker = (focus_ == i) ? '>' : ' ';
        char line[16];
        snprintf(line, sizeof(line), "%c %s", marker, rows[i].label);
        onebit::drawBitmapText(fb, font, 8, y, line, onebit::BLACK);
        rows[i].ti->render(fb, font, 100, y - 2, fb.width() - 110);
        y += font.glyph_height + 6;
    }

    // Key row: resolve ssh_key_id via KeyStore, fall back to "(password)".
    const char* key_display = "(password)";
    if (index_ >= 0 && index_ < ctx_.configMgr.serverCount()) {
        const auto& existing = ctx_.configMgr.getServer(index_).creds;
        if (existing.use_key_auth && existing.ssh_key_id[0] != '\0') {
            auto parsed = ssh_keys::KeyId::parse(existing.ssh_key_id);
            if (parsed) {
                const auto* meta = ctx_.keyStore.find(*parsed);
                key_display = meta ? meta->name : "(missing)";
            } else {
                key_display = "(invalid)";
            }
        }
    }
    {
        char marker = (focus_ == 5) ? '>' : ' ';
        char line[16];
        snprintf(line, sizeof(line), "%c %s", marker, "Key:      ");
        onebit::drawBitmapText(fb, font, 8, y, line, onebit::BLACK);
        onebit::drawBitmapText(fb, font, 100, y, key_display, onebit::BLACK);
        y += font.glyph_height + 6;
    }

    y += 8;
    const char* save = "[ Save ]";
    const char* cancel = "[ Cancel ]";
    auto drawBtn = [&](int16_t x, const char* txt, bool sel) {
        int16_t w = onebit::getBitmapTextWidth(font, txt) + 4;
        if (sel) {
            onebit::fillRect(fb, x - 2, y - 2, w, font.glyph_height + 4,
                             onebit::BLACK);
            onebit::drawBitmapText(fb, font, x, y, txt, onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, x, y, txt, onebit::BLACK);
        }
    };
    drawBtn(20, save, focus_ == 6);
    drawBtn(140, cancel, focus_ == 7);

    const char* hint = (focus_ == 5)
        ? "Enter pick key  Up/Dn nav  Esc cancel"
        : "Tab next  Up/Dn nav  Ctrl+R reveal pw  Esc cancel";
    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        hint, onebit::BLACK);
}

} // namespace app
