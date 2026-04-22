#include "screens/ssh_key_import_screen.hpp"
#include "screens/text_input_screen.hpp"
#include "screen_stack.hpp"
#include "overlay.hpp"
#include "ssh_keys.hpp"
#include "ssh_key_codec.hpp"
#include "ssh_key_types.hpp"

#include <1bit/render/primitives.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "libssh/libssh.h"

namespace app {

static constexpr const char* SD_IMPORT_DIR = "/sdcard/ssh_keys";

static const char* type_glyph_short(ssh_keys::KeyType t, uint16_t rsa_bits) {
    return ssh_keys::key_type_glyph(t, rsa_bits);
}

SshKeyImportScreen::SshKeyImportScreen(ScreenContext& ctx) : ctx_(ctx) {}

void SshKeyImportScreen::onEnter() {
    candidates_.clear();
    skipped_ = 0;
    sel_ = 0;
    doScan();
    scanned_ = true;
}

void SshKeyImportScreen::doScan() {
    DIR* dir = opendir(SD_IMPORT_DIR);
    if (!dir) {
        // No dir is not an error — just empty list.
        return;
    }
    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        // Skip .pub and .tmp
        const char* base = ent->d_name;
        size_t blen = std::strlen(base);
        if (blen >= 4 && std::strcmp(base + blen - 4, ".pub") == 0) continue;
        if (blen >= 4 && std::strcmp(base + blen - 4, ".tmp") == 0) continue;

        // d_name is up to NAME_MAX (255) + NUL. Pad the buffer to
        // accommodate the directory prefix + '/' + basename + NUL so the
        // compiler's format-truncation analysis stays quiet.
        char full[512];
        std::snprintf(full, sizeof(full), "%s/%s", SD_IMPORT_DIR, base);

        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        ssh_key key = nullptr;
        int rc = ssh_pki_import_privkey_file(full, nullptr, nullptr,
                                              nullptr, &key);
        if (rc != SSH_OK || !key) {
            ++skipped_;
            continue;
        }
        uint16_t rsa_bits = 0;
        auto kt = ssh_keys::ssh_key_to_type(key, rsa_bits);
        ssh_key_free(key);

        Candidate c;
        c.filename = base;
        c.full_path = full;
        c.type = kt;
        c.rsa_bits = rsa_bits;
        candidates_.push_back(std::move(c));
    }
    closedir(dir);
}

void SshKeyImportScreen::beginNamePrompt(ScreenStack& stack) {
    if (sel_ < 0 || sel_ >= (int)candidates_.size()) return;
    Candidate cand = candidates_[sel_];
    // Prefill with the filename stem (strip any trailing extension).
    std::string stem = cand.filename;
    auto dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem.erase(dot);
    if (stem.size() > 30) stem.resize(30);

    stack.push(std::make_unique<TextInputScreen>(
        ctx_,
        "Name imported key",
        stem.c_str(),
        [this, cand](TextInputResult r, const std::string& name) {
            if (r != TextInputResult::Submit || name.empty()) return;
            doImport(cand, name);
        }));
}

void SshKeyImportScreen::doImport(const Candidate& cand,
                                   const std::string& name) {
    // One-time plaintext warning.
    if (ctx_.keyStore.warn_plaintext_needed()) {
        Candidate c_copy = cand;
        std::string n_copy = name;
        ctx_.overlay.showConfirm(
            "Plaintext keys",
            "Private keys live in plaintext on internal flash. "
            "Don't store production keys here. Continue?",
            [this, c_copy, n_copy](bool yes) {
                if (!yes) return;
                // warn_plaintext_needed is now false; recurse.
                doImport(c_copy, n_copy);
            });
        return;
    }

    ssh_key key = nullptr;
    int rc = ssh_pki_import_privkey_file(cand.full_path.c_str(), nullptr,
                                          nullptr, nullptr, &key);
    if (rc != SSH_OK || !key) {
        ctx_.overlay.showError("Import failed",
                                "libssh rejected the key — see logs.");
        return;
    }

    ssh_keys::KeyMeta tmpl;
    std::strncpy(tmpl.name, name.c_str(), sizeof(tmpl.name) - 1);
    tmpl.name[sizeof(tmpl.name) - 1] = '\0';
    tmpl.created_utc = 0;  // imports don't have a "created on device" time

    ssh_keys::KeyId new_id = ctx_.keyStore.add(tmpl, key);
    ssh_key_free(key);

    if (new_id.hex() == ssh_keys::KeyId{}.hex()) {
        ctx_.overlay.showError("Import failed",
                                "store full, NVS/FS error, or duplicate.");
        return;
    }

    // Offer to delete the source file from SD.
    std::string path_copy = cand.full_path;
    char body[96];
    std::snprintf(body, sizeof(body),
                   "Delete source '%s' from SD?",
                   cand.filename.c_str());
    ctx_.overlay.showConfirm("Delete source?", body, [this, path_copy](bool yes) {
        if (!yes) return;
        if (unlink(path_copy.c_str()) != 0) {
            ctx_.overlay.showError("Unlink failed", path_copy.c_str());
            return;
        }
        // Rescan: refresh the candidate list so the deleted entry drops.
        candidates_.clear();
        skipped_ = 0;
        sel_ = 0;
        doScan();
    });
    ctx_.overlay.showToast("Imported", 1500);
}

void SshKeyImportScreen::handleInput(const input::InputEvent& evt,
                                      ScreenStack& stack) {
    if (evt.source != input::Source::Keyboard ||
        evt.type   != input::EventType::Keypress) return;

    int count = static_cast<int>(candidates_.size());

    if (evt.data_length == 3 && evt.data[0] == 0x1B && evt.data[1] == '[') {
        if (count > 0) {
            if (evt.data[2] == 'A') sel_ = (sel_ - 1 + count) % count;
            if (evt.data[2] == 'B') sel_ = (sel_ + 1) % count;
        }
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == '\r') {
        if (count > 0) beginNamePrompt(stack);
        return;
    }
    if (evt.data_length == 1 && evt.data[0] == 0x1B) {
        stack.pop();
        return;
    }
}

void SshKeyImportScreen::render(onebit::IFramebuffer& fb,
                                 const onebit::BitmapFont& font) {
    onebit::drawBitmapText(fb, font, 10, 8,
                            "Import Key (SD)", onebit::BLACK);
    onebit::fillRect(fb, 10, 8 + font.glyph_height + 2,
                      fb.width() - 20, 1, onebit::BLACK);

    int16_t y = 8 + font.glyph_height + 10;
    const int16_t row_h = font.glyph_height + 4;

    if (!scanned_) {
        onebit::drawBitmapText(fb, font, 10, y, "Scanning...", onebit::BLACK);
        return;
    }

    if (candidates_.empty()) {
        char msg[96];
        std::snprintf(msg, sizeof(msg),
                       "No recognized keys in %s.", SD_IMPORT_DIR);
        onebit::drawBitmapText(fb, font, 10, y, msg, onebit::BLACK);
        if (skipped_ > 0) {
            char smsg[64];
            std::snprintf(smsg, sizeof(smsg),
                           "(%d file%s skipped — libssh could not parse)",
                           skipped_, skipped_ == 1 ? "" : "s");
            onebit::drawBitmapText(fb, font, 10, y + row_h, smsg, onebit::BLACK);
        }
    } else {
        for (size_t i = 0; i < candidates_.size(); ++i) {
            if (y + font.glyph_height > fb.height() - 20) break;
            const auto& c = candidates_[i];
            char line[128];
            std::snprintf(line, sizeof(line), "%c %s  [%s]",
                           (int)i == sel_ ? '>' : ' ',
                           c.filename.c_str(),
                           type_glyph_short(c.type, c.rsa_bits));
            if ((int)i == sel_) {
                onebit::fillRect(fb, 8, y - 1, fb.width() - 16,
                                  font.glyph_height + 2, onebit::BLACK);
                onebit::drawBitmapText(fb, font, 10, y, line, onebit::WHITE);
            } else {
                onebit::drawBitmapText(fb, font, 10, y, line, onebit::BLACK);
            }
            y += row_h;
        }
        if (skipped_ > 0) {
            char smsg[64];
            std::snprintf(smsg, sizeof(smsg), "(%d skipped)", skipped_);
            onebit::drawBitmapText(fb, font, 10, y + 2, smsg, onebit::BLACK);
        }
    }

    onebit::drawBitmapText(fb, font, 10, fb.height() - font.glyph_height - 4,
        "Up/Dn  Enter import  Esc back", onebit::BLACK);
}

} // namespace app
