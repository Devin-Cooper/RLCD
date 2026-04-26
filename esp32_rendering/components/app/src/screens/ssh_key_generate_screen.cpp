#include "screens/ssh_key_generate_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_export.hpp"
#include "wifi_manager.hpp"

#include <1bit/render/primitives.hpp>

#include <array>
#include <cstdio>
#include <cstring>

namespace app {

namespace {
constexpr std::array<app::KeybindHint, 3> kHints = {{
    {"Enter",  "generate"},
    {"Esc",    "cancel"},
    {"Ctrl+/", "help"},
}};
static_assert(sizeof("Ctrl+/") <= 12 && sizeof("generate") <= 16,
              "kHints contains a string longer than KeybindHint capacity");
} // namespace

app::SpanView<const app::KeybindHint> SshKeyGenerateScreen::keybindHints() const {
    return kHints;
}

SshKeyGenerateScreen::SshKeyGenerateScreen(ScreenContext& ctx)
    : ctx_(ctx), input_(name_buf_, sizeof(name_buf_), TextInputOpts{}) {}

void SshKeyGenerateScreen::onEnter() {
    // Wi-Fi preflight (spec decision 5) — keygen needs hardware RNG
    // entropy, so we require the radio to be up.
    if (ctx_.wifiMgr.connectionInfo().state != wifi::State::Connected) {
        ctx_.overlay.showError(
            "Wi-Fi required",
            "Key generation needs Wi-Fi for RNG entropy. Connect first.");
        // User dismisses the modal; leave the screen visible with a
        // message. Esc pops.
        wifi_ok_ = false;
        return;
    }
    wifi_ok_ = true;
}

void SshKeyGenerateScreen::submit(ScreenStack& stack) {
    if (name_buf_[0] == '\0') return;  // empty name → ignore

    // One-time plaintext warning. If warn_plaintext_needed() returns true
    // we show the confirm; on Yes we re-enter submit() (now a no-op warn)
    // and proceed to generate. On No we cancel and leave the screen up.
    if (ctx_.keyStore.warn_plaintext_needed()) {
        ctx_.overlay.showConfirm(
            "Plaintext keys",
            "Private keys live in plaintext on internal flash. "
            "Don't store production keys here. Continue?",
            [this, &stack](bool yes) {
                if (!yes) return;
                submit(stack);
            });
        return;
    }

    generating_ = true;
    ctx_.overlay.showToast("Generating...", 500);

    ssh_keys::KeyId new_id;
    auto rc = ssh_keys::generate_ed25519(ctx_.keyStore, ctx_.wifiMgr,
                                          name_buf_, /*now_utc=*/0, new_id);
    generating_ = false;

    switch (rc) {
        case ssh_keys::GenerateResult::Ok: {
            char msg[80];
            std::snprintf(msg, sizeof(msg), "Generated %s", name_buf_);
            ctx_.overlay.showToast(msg, 2000);
            stack.pop();
            break;
        }
        case ssh_keys::GenerateResult::WifiDown:
            ctx_.overlay.showError("Wi-Fi dropped", "Connect and retry.");
            break;
        case ssh_keys::GenerateResult::LibsshError:
            ctx_.overlay.showError("Keygen failed",
                                    "libssh error — see logs.");
            break;
        case ssh_keys::GenerateResult::StorePushFailed:
            ctx_.overlay.showError("Storage failed",
                                    "NVS or LittleFS write error.");
            break;
        case ssh_keys::GenerateResult::IndexFull:
            ctx_.overlay.showError("Key store full",
                                    "Delete a key first.");
            break;
    }
}

void SshKeyGenerateScreen::handleInput(const input::InputEvent& evt,
                                        ScreenStack& stack) {
    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) {
        return;
    }

    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
        return;
    }

    if (!wifi_ok_ || generating_) return;

    TextInputResult r = input_.handleKey(evt.data, evt.data_length);
    if (r == TextInputResult::Submit) {
        submit(stack);
    } else if (r == TextInputResult::Cancel) {
        stack.pop();
    }
}

void SshKeyGenerateScreen::render(onebit::IFramebuffer& fb,
                                   const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8,
                            "Generate Ed25519 Key", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 12;
    if (!wifi_ok_) {
        onebit::drawBitmapText(fb, font, 10, y,
                                "Wi-Fi required. Esc to return.",
                                onebit::BLACK);
        return;
    }

    onebit::drawBitmapText(fb, font, 10, y, "Name:", onebit::BLACK);
    input_.render(fb, font, 10, y + font.glyph_height + 4,
                   fb.width() - 20);

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Enter generate  Esc cancel", onebit::BLACK);
}

} // namespace app
