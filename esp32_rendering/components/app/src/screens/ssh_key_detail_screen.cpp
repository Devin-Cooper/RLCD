#include "screens/ssh_key_detail_screen.hpp"
#include "screens/ssh_key_pubkey_text_screen.hpp"
#include "screens/ssh_key_pubkey_qr_screen.hpp"
#include "screens/ssh_key_enroll_screen.hpp"
#include "screens/text_input_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_export.hpp"
#include "ssh_key_types.hpp"
#include "config_manager.hpp"

#include <1bit/render/primitives.hpp>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace app {

static const char* type_full_name(ssh_keys::KeyType t, uint16_t rsa_bits) {
    switch (t) {
        case ssh_keys::KeyType::Ed25519:   return "ed25519";
        case ssh_keys::KeyType::EcdsaP256: return "ecdsa-p256";
        case ssh_keys::KeyType::EcdsaP384: return "ecdsa-p384";
        case ssh_keys::KeyType::EcdsaP521: return "ecdsa-p521";
        case ssh_keys::KeyType::Rsa:
            switch (rsa_bits) {
                case 2048: return "rsa-2048";
                case 3072: return "rsa-3072";
                case 4096: return "rsa-4096";
                default:   return "rsa";
            }
    }
    return "?";
}

SshKeyDetailScreen::SshKeyDetailScreen(ScreenContext& ctx,
                                        const ssh_keys::KeyId& id)
    : ctx_(ctx), id_(id) {}

void SshKeyDetailScreen::onEnter() {}

void SshKeyDetailScreen::beginRename(ScreenStack& stack) {
    const auto* meta = ctx_.keyStore.find(id_);
    if (!meta) return;
    auto id = id_;
    stack.push(std::make_unique<TextInputScreen>(
        ctx_,
        "Rename key",
        meta->name,
        [this, id](TextInputResult r, const std::string& new_name) {
            if (r != TextInputResult::Submit || new_name.empty()) return;
            if (!ctx_.keyStore.rename(id, new_name.c_str())) {
                ctx_.overlay.showError("Rename failed",
                                        "name collision or NVS error");
            }
        }));
}

void SshKeyDetailScreen::confirmDelete() {
    const auto* meta = ctx_.keyStore.find(id_);
    if (!meta) return;
    auto id = id_;
    char kname[32];
    std::strncpy(kname, meta->name, sizeof(kname) - 1);
    kname[sizeof(kname) - 1] = '\0';

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
        if (ok) {
            ctx_.overlay.showToast("Deleted", 1500);
            ctx_.stack.pop();  // return to list
        } else {
            ctx_.overlay.showError("Delete failed", "NVS write error");
        }
    });
}

void SshKeyDetailScreen::doSdExport() {
    char err[64] = {};
    bool ok = ssh_keys::export_sd(ctx_.keyStore, id_, err, sizeof(err));
    if (ok) {
        ctx_.overlay.showToast("Exported to SD", 2000);
    } else {
        ctx_.overlay.showError("SD export failed",
                                err[0] ? err : "see logs");
    }
}

void SshKeyDetailScreen::handleInput(const input::InputEvent& evt,
                                      ScreenStack& stack) {
    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) return;

    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
        return;
    }
    if (evt.data_length != 1) return;

    char c = evt.data[0];
    if (c == 't' || c == 'T') {
        stack.push(std::make_unique<SshKeyPubkeyTextScreen>(ctx_, id_));
    } else if (c == 'q' || c == 'Q') {
        stack.push(std::make_unique<SshKeyPubkeyQrScreen>(ctx_, id_));
    } else if (c == 's' || c == 'S') {
        doSdExport();
    } else if (c == 'e' || c == 'E') {
        stack.push(std::make_unique<SshKeyEnrollScreen>(ctx_, id_));
    } else if (c == 'r' || c == 'R') {
        beginRename(stack);
    } else if (c == 'd' || c == 'D') {
        confirmDelete();
    }
}

void SshKeyDetailScreen::render(onebit::IFramebuffer& fb,
                                 const onebit::BitmapFont& font) {
    const auto* meta = ctx_.keyStore.find(id_);
    onebit::drawBitmapText(fb, font, 10, 8, "Key Detail", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 10;
    const int16_t row_h = font.glyph_height + 3;

    if (!meta) {
        onebit::drawBitmapText(fb, font, 10, y,
                                "Key not found. Esc to return.",
                                onebit::BLACK);
        return;
    }

    char line[128];

    std::snprintf(line, sizeof(line), "Name:    %s", meta->name);
    onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
    y += row_h;

    std::snprintf(line, sizeof(line), "Type:    %s",
                   type_full_name(meta->type, meta->rsa_bits));
    onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
    y += row_h;

    if (meta->created_utc == 0) {
        std::snprintf(line, sizeof(line), "Created: -");
    } else {
        std::time_t t = static_cast<std::time_t>(meta->created_utc);
        std::tm tmv{};
        gmtime_r(&t, &tmv);
        std::snprintf(line, sizeof(line),
                       "Created: %04d-%02d-%02d %02d:%02d",
                       1900 + tmv.tm_year, 1 + tmv.tm_mon, tmv.tm_mday,
                       tmv.tm_hour, tmv.tm_min);
    }
    onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
    y += row_h;

    std::snprintf(line, sizeof(line), "Used:    %u",
                   (unsigned)meta->use_count);
    onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
    y += row_h;

    // Fingerprint as "SHA256:<43-char-b64>", matching `ssh-keygen -lf`.
    onebit::drawBitmapText(fb, font, 10, y, "Fingerprint:", onebit::BLACK);
    y += row_h;
    char fp_str[64] = {};
    if (ssh_keys::fp_sha256_b64(meta->fp_sha256, fp_str, sizeof(fp_str))) {
        onebit::drawBitmapText(fb, font, 10, y, fp_str, onebit::BLACK);
    } else {
        onebit::drawBitmapText(fb, font, 10, y,
                                "SHA256: (encode error)", onebit::BLACK);
    }
    y += row_h;

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "T text  Q QR  S SD  E enroll  R rename  D del  Esc",
        onebit::BLACK);
}

} // namespace app
