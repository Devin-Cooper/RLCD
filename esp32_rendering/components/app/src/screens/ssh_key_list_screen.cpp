#include "screens/ssh_key_list_screen.hpp"
#include "screens/ssh_key_generate_screen.hpp"
#include "screens/ssh_key_import_screen.hpp"
#include "screens/ssh_key_detail_screen.hpp"
#include "screens/text_input_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_types.hpp"
#include "config_manager.hpp"

#include <1bit/render/primitives.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <esp_timer.h>

namespace app {

namespace {
constexpr std::array<app::KeybindHint, 7> kHints = {{
    {"[up/dn]", "nav"},
    {"Enter",   "detail"},
    {"G",       "generate"},
    {"I",       "import"},
    {"Esc",     "back"},
    {"Sh-D",    "delete"},
    {"Ctrl+/",  "help"},
}};
static_assert(sizeof("[up/dn]") <= 12 && sizeof("generate") <= 16,
              "kHints contains a string longer than KeybindHint capacity");
} // namespace

app::SpanView<const app::KeybindHint> SshKeyListScreen::keybindHints() const {
    return kHints;
}

SshKeyListScreen::SshKeyListScreen(ScreenContext& ctx)
    : ctx_(ctx), mode_(Mode::Browse) {}

SshKeyListScreen::SshKeyListScreen(ScreenContext& ctx,
                                    std::function<void(const std::string&)> on_pick)
    : ctx_(ctx), mode_(Mode::Picker), on_pick_(std::move(on_pick)) {}

int16_t SshKeyListScreen::computeRowY(int index) const {
    return list_start_y_ + static_cast<int16_t>(index) * row_h_;
}

void SshKeyListScreen::onSelectionChange(int old_index, int new_index) {
    if (old_index == new_index) return;
    if (!focus_y_initialized_) return;
    int16_t old_y = computeRowY(old_index);
    int16_t new_y = computeRowY(new_index);
    auto tag = makeTag(TweenKind::FocusRect, focus_id::SshKeyListScreen);
    ctx_.animator.start(tag, old_y, new_y, kFocusRectUs, esp_timer_get_time());
    prev_selected_y_ = new_y;
}

void SshKeyListScreen::onEnter() {
    sel_ = 0;
    focus_y_initialized_ = false;
    auto tag = makeTag(TweenKind::FocusRect, focus_id::SshKeyListScreen);
    ctx_.animator.cancel(tag);
}

int SshKeyListScreen::rowCount() const {
    int n = static_cast<int>(ctx_.keyStore.keys().size());
    return (mode_ == Mode::Picker) ? n + 1 : n;
}

void SshKeyListScreen::openDetail(ScreenStack& stack) {
    const auto& keys = ctx_.keyStore.keys();
    if (sel_ < 0 || sel_ >= (int)keys.size()) return;
    stack.push(std::make_unique<SshKeyDetailScreen>(ctx_, keys[sel_].id));
}

void SshKeyListScreen::beginRename(ScreenStack& stack) {
    const auto& keys = ctx_.keyStore.keys();
    if (mode_ != Mode::Browse) return;
    if (sel_ < 0 || sel_ >= (int)keys.size()) return;
    auto id = keys[sel_].id;
    stack.push(std::make_unique<TextInputScreen>(
        ctx_,
        "Rename key",
        keys[sel_].name,
        [this, id](TextInputResult r, const std::string& new_name) {
            if (r != TextInputResult::Submit || new_name.empty()) return;
            if (!ctx_.keyStore.rename(id, new_name.c_str())) {
                ctx_.overlay.showError("Rename failed",
                                        "name collision or NVS error");
            }
        }));
}

void SshKeyListScreen::confirmDelete() {
    const auto& keys = ctx_.keyStore.keys();
    if (mode_ != Mode::Browse) return;
    if (sel_ < 0 || sel_ >= (int)keys.size()) return;
    auto id = keys[sel_].id;
    // Copy name — keys vector may be mutated before the confirm callback fires.
    char kname[32];
    std::strncpy(kname, keys[sel_].name, sizeof(kname) - 1);
    kname[sizeof(kname) - 1] = '\0';

    // Gather referenced-by server list.
    std::vector<std::string> refs;
    int scount = ctx_.configMgr.serverCount();
    std::string hex = id.hex();
    for (int i = 0; i < scount; ++i) {
        const auto& c = ctx_.configMgr.getServer(i).creds;
        if (c.use_key_auth && std::strncmp(c.ssh_key_id, hex.c_str(), 32) == 0) {
            refs.emplace_back(c.name);
        }
    }
    if (!refs.empty()) {
        std::string body = "Key in use by: ";
        for (size_t i = 0; i < refs.size(); ++i) {
            if (i) body += ", ";
            body += refs[i];
        }
        body += ". Remove those first.";
        ctx_.overlay.showError("Cannot delete", body.c_str());
        return;
    }

    char body[80];
    std::snprintf(body, sizeof(body), "Delete key '%s'?", kname);
    ctx_.overlay.showConfirm("Confirm", body, [this, id](bool yes) {
        if (!yes) return;
        bool ok = ctx_.keyStore.delete_key(id, nullptr, nullptr);
        if (ok) ctx_.overlay.showToast("Deleted", 1500);
        else    ctx_.overlay.showError("Delete failed", "NVS write error");
        int max_idx = rowCount() - 1;
        if (sel_ > max_idx) sel_ = max_idx < 0 ? 0 : max_idx;
        // Row count changed; cancel tween and re-init y.
        auto tag = makeTag(TweenKind::FocusRect,
                           focus_id::SshKeyListScreen);
        ctx_.animator.cancel(tag);
        focus_y_initialized_ = false;
    });
}

void SshKeyListScreen::firePickAndPop(ScreenStack& stack) {
    // Snapshot the value we want to hand back, then pop FIRST so the
    // callback can push a follow-up screen without the deferred pop
    // eating it.
    std::string pick;
    const auto& keys = ctx_.keyStore.keys();
    if (sel_ == 0) {
        pick = "";  // synthetic "(password auth)" row
    } else {
        int idx = sel_ - 1;
        if (idx >= 0 && idx < (int)keys.size()) {
            pick = keys[idx].id.hex();
        }
    }
    auto cb = std::move(on_pick_);
    stack.pop();
    if (cb) cb(pick);
}

void SshKeyListScreen::handleInput(const input::InputEvent& evt,
                                    ScreenStack& stack) {
    int count = rowCount();

    if (evt.source == input::Source::Keyboard &&
        evt.type == input::EventType::Keypress) {
        // Arrow keys: 3-byte CSI
        if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
            if (count > 0) {
                int old_sel = sel_;
                if (evt.data[2] == 'A') sel_ = (sel_ - 1 + count) % count;
                if (evt.data[2] == 'B') sel_ = (sel_ + 1) % count;
                if (evt.data[2] == 'A' || evt.data[2] == 'B')
                    onSelectionChange(old_sel, sel_);
            }
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == '\r') {
            if (mode_ == Mode::Picker) {
                firePickAndPop(stack);
            } else {
                openDetail(stack);
            }
            return;
        }
        if (evt.data_length == 1 && evt.data[0] == 0x1B) {
            stack.pop();
            return;
        }
        if (mode_ != Mode::Browse) return;
        if (evt.data_length == 1) {
            char c = evt.data[0];
            if (c == 'n' || c == 'N') {
                stack.push(std::make_unique<SshKeyGenerateScreen>(ctx_));
                return;
            }
            if (c == 'i' || c == 'I') {
                stack.push(std::make_unique<SshKeyImportScreen>(ctx_));
                return;
            }
            if (c == 'r' || c == 'R') { beginRename(stack); return; }
            if (c == 'd' || c == 'D') { confirmDelete(); return; }
        }
    }

    // Button fallback: A=up, B=down, long-A=Esc, long-B=Enter
    if (evt.source == input::Source::Button) {
        if (evt.type == input::EventType::ButtonShort && count > 0) {
            int old_sel = sel_;
            if (evt.button_id == 0) sel_ = (sel_ - 1 + count) % count;
            if (evt.button_id == 1) sel_ = (sel_ + 1) % count;
            onSelectionChange(old_sel, sel_);
        }
        if (evt.type == input::EventType::ButtonLong) {
            if (evt.button_id == 0) stack.pop();
            if (evt.button_id == 1) {
                if (mode_ == Mode::Picker) {
                    firePickAndPop(stack);
                } else {
                    openDetail(stack);
                }
            }
        }
    }
}

void SshKeyListScreen::render(onebit::IFramebuffer& fb,
                               const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8,
                            mode_ == Mode::Picker ? "Select SSH Key" : "SSH Keys",
                            onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 8;

    // Cache list layout for the focus-rect animation.
    list_start_y_ = y - 1;
    row_h_        = font.glyph_height + 4;

    // Compute the focus-rect y. Animate if a tween is in progress.
    auto tag = makeTag(TweenKind::FocusRect, focus_id::SshKeyListScreen);
    int64_t now = esp_timer_get_time();
    int row_total = rowCount();
    if (row_total > 0 && !focus_y_initialized_) {
        prev_selected_y_ = computeRowY(sel_);
        focus_y_initialized_ = true;
    }
    int16_t cur_y = ctx_.animator.inProgress(tag, now)
                  ? ctx_.animator.value(tag, now)
                  : prev_selected_y_;
    if (row_total > 0) {
        onebit::fillRect(fb, 8, cur_y, fb.width() - 16,
                         font.glyph_height + 2, onebit::BLACK);
    }

    int row = 0;

    if (mode_ == Mode::Picker) {
        char line[48];
        std::snprintf(line, sizeof(line), "%c (password auth)",
                       row == sel_ ? '>' : ' ');
        onebit::drawBitmapText(fb, font, 10, y, line,
                               row == sel_ ? onebit::WHITE : onebit::BLACK);
        y += font.glyph_height + 4;
        ++row;
    }

    const auto& keys = ctx_.keyStore.keys();
    for (size_t i = 0; i < keys.size(); ++i, ++row) {
        if (y + font.glyph_height > fb.height() - 20) break;
        const auto& m = keys[i];
        int ref_n = 0;
        int scount = ctx_.configMgr.serverCount();
        std::string hex = m.id.hex();
        for (int s = 0; s < scount; ++s) {
            const auto& c = ctx_.configMgr.getServer(s).creds;
            if (c.use_key_auth &&
                std::strncmp(c.ssh_key_id, hex.c_str(), 32) == 0) ++ref_n;
        }
        char fp_head[9] = {};
        for (int k = 0; k < 4; ++k) {
            std::snprintf(fp_head + k * 2, 3, "%02x", m.fp_sha256[k]);
        }
        char line[128];
        std::snprintf(line, sizeof(line), "%c %s %s  %s  (used %d)",
                       row == sel_ ? '>' : ' ',
                       m.name,
                       ssh_keys::key_type_glyph(m.type, m.rsa_bits),
                       fp_head, ref_n);
        onebit::drawBitmapText(fb, font, 10, y, line,
                               row == sel_ ? onebit::WHITE : onebit::BLACK);
        y += font.glyph_height + 4;
    }

    if (mode_ == Mode::Browse && keys.empty()) {
        onebit::drawBitmapText(fb, font, 10, y + 4,
                                "No keys.  N = generate,  I = import.",
                                onebit::BLACK);
    }

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        mode_ == Mode::Picker
            ? "Up/Dn nav  Enter pick  Esc cancel"
            : "Up/Dn  Enter  N new  I import  R rename  D delete  Esc",
        onebit::BLACK);
}

} // namespace app
