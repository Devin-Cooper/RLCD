#include "screens/ssh_key_enroll_screen.hpp"
#include "screens/text_input_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_export.hpp"
#include "config_manager.hpp"

#include <1bit/render/primitives.hpp>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace app {

SshKeyEnrollScreen::SshKeyEnrollScreen(ScreenContext& ctx,
                                        const ssh_keys::KeyId& id)
    : ctx_(ctx), id_(id) {}

void SshKeyEnrollScreen::collectEligible() {
    eligible_.clear();
    int n = ctx_.configMgr.serverCount();
    for (int i = 0; i < n; ++i) {
        const auto& c = ctx_.configMgr.getServer(i).creds;
        if (!c.use_key_auth) eligible_.push_back(i);
    }
}

void SshKeyEnrollScreen::onEnter() {
    collectEligible();
    if (eligible_.empty()) {
        ctx_.overlay.showError(
            "No eligible servers",
            "All servers already use key auth, or no servers exist.");
        ctx_.stack.pop();
        return;
    }
    sel_ = 0;
    state_ = State::Picker;
    needs_run_ = false;
    chosen_server_idx_ = -1;
}

void SshKeyEnrollScreen::beginPasswordPrompt(ScreenStack& stack) {
    if (sel_ < 0 || sel_ >= (int)eligible_.size()) return;
    chosen_server_idx_ = eligible_[sel_];
    const auto& c = ctx_.configMgr.getServer(chosen_server_idx_).creds;

    // username is char[32] and host is char[64] — worst case 32 + "@" + 64 + prefix.
    char prompt[128];
    std::snprintf(prompt, sizeof(prompt), "Password for %s@%s",
                   c.username, c.host);

    TextInputOpts opts;
    opts.masked = true;
    opts.tab_toggles_reveal = true;

    stack.push(std::make_unique<TextInputScreen>(
        ctx_,
        prompt,
        "",
        [this](TextInputResult r, const std::string& pw) {
            if (r != TextInputResult::Submit || pw.empty()) {
                // Cancel: stay on the picker.
                return;
            }
            // Defer the actual enroll run to the next render tick via a
            // needs_run_ flag so this callback (which fires inside the
            // child Screen's pop flow) doesn't reenter the overlay
            // machinery with the password-screen still partly torn down.
            // We stash the password in a member for runEnroll to consume.
            pending_password_ = pw;
            needs_run_ = true;
            state_ = State::Running;
        },
        opts));
}

void SshKeyEnrollScreen::runEnroll(const std::string& password) {
    char err[128] = {};
    ctx_.overlay.showToast("Enrolling...", 500);

    auto rc = ssh_keys::enroll_key(ctx_.keyStore, id_,
                                    static_cast<void*>(&ctx_.configMgr),
                                    chosen_server_idx_, password.c_str(),
                                    err, sizeof(err));

    // Zero the password buffer held in the screen member.
    pending_password_.assign(pending_password_.size(), '\0');
    pending_password_.clear();

    const auto& srv = ctx_.configMgr.getServer(chosen_server_idx_).creds;

    switch (rc) {
        case ssh_keys::EnrollResult::Ok: {
            char msg[80];
            std::snprintf(msg, sizeof(msg), "Enrolled %s", srv.name);
            ctx_.overlay.showToast(msg, 2000);
            ctx_.configMgr.markRepicked(chosen_server_idx_);
            state_ = State::Done;
            ctx_.stack.pop();
            break;
        }
        case ssh_keys::EnrollResult::AlreadyEnrolled: {
            ctx_.overlay.showToast("Already enrolled — record updated",
                                    2000);
            ctx_.configMgr.markRepicked(chosen_server_idx_);
            state_ = State::Done;
            ctx_.stack.pop();
            break;
        }
        case ssh_keys::EnrollResult::ProbeOrPassword:
            ctx_.overlay.showError("Probe / password failed",
                                    err[0] ? err : "bad password or network");
            state_ = State::Picker;
            break;
        case ssh_keys::EnrollResult::Upload:
            ctx_.overlay.showError("Upload failed",
                                    err[0] ? err : "remote shell error");
            state_ = State::Picker;
            break;
        case ssh_keys::EnrollResult::Verify:
            ctx_.overlay.showError("Verify failed",
                                    err[0] ? err : "key rejected after upload");
            state_ = State::Picker;
            break;
        case ssh_keys::EnrollResult::FlipFailed:
            ctx_.overlay.showError("Save failed",
                                    err[0] ? err : "NVS write error");
            state_ = State::Picker;
            break;
    }
}

void SshKeyEnrollScreen::handleInput(const input::InputEvent& evt,
                                      ScreenStack& stack) {
    if (state_ == State::Running || state_ == State::Done) {
        // Deferred run: if we just transitioned to Running via the
        // TextInputScreen callback, fire the enroll now. handleInput is
        // the first place the parent screen gets a tick after the child
        // Screen pops.
        if (needs_run_ && state_ == State::Running) {
            needs_run_ = false;
            runEnroll(pending_password_);
        }
        return;
    }

    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) return;

    int count = static_cast<int>(eligible_.size());

    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        if (count > 0) {
            if (evt.data[2] == 'A') sel_ = (sel_ - 1 + count) % count;
            if (evt.data[2] == 'B') sel_ = (sel_ + 1) % count;
        }
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == '\r') {
        if (count > 0) beginPasswordPrompt(stack);
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
        return;
    }
}

void SshKeyEnrollScreen::render(onebit::IFramebuffer& fb,
                                 const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8, "Enroll Key", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 10;
    const int16_t row_h = font.glyph_height + 4;

    if (state_ == State::Running) {
        onebit::drawBitmapText(fb, font, 10, y,
                                "Enrolling... (this can take ~20s)",
                                onebit::BLACK);
        return;
    }

    if (eligible_.empty()) {
        onebit::drawBitmapText(fb, font, 10, y,
                                "No password-auth servers available.",
                                onebit::BLACK);
        onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
                                "Esc to return", onebit::BLACK);
        return;
    }

    onebit::drawBitmapText(fb, font, 10, y,
                            "Select server to enroll:", onebit::BLACK);
    y += row_h;

    for (size_t i = 0; i < eligible_.size(); ++i) {
        if (y + font.glyph_height > fb.height() - 20) break;
        int idx = eligible_[i];
        const auto& c = ctx_.configMgr.getServer(idx).creds;
        // name(32) + user(32) + host(64) + port(5) + fixed chars
        char line[160];
        std::snprintf(line, sizeof(line), "%c %s (%s@%s:%d)",
                       (int)i == sel_ ? '>' : ' ',
                       c.name, c.username, c.host, c.port);
        if ((int)i == sel_) {
            onebit::fillRect(fb, 8, y - 1, fb.width() - 16,
                              font.glyph_height + 2, onebit::BLACK);
            onebit::drawBitmapText(fb, font, 10, y, line, onebit::WHITE);
        } else {
            onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
        }
        y += row_h;
    }

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Up/Dn  Enter enroll  Esc back", onebit::BLACK);
}

} // namespace app
