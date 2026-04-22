#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "ssh_key_types.hpp"

#include <string>
#include <vector>

namespace app {

/// SD-card import: scans `/sdcard/ssh_keys/` for private-key files,
/// previews each via libssh's PKI parser, and lets the user pick one to
/// name and add to the KeyStore.
class SshKeyImportScreen : public Screen {
public:
    explicit SshKeyImportScreen(ScreenContext& ctx);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

private:
    struct Candidate {
        std::string filename;     // basename, e.g. "id_ed25519"
        std::string full_path;    // e.g. "/sdcard/ssh_keys/id_ed25519"
        ssh_keys::KeyType type = ssh_keys::KeyType::Ed25519;
        uint16_t rsa_bits = 0;
    };

    ScreenContext& ctx_;
    std::vector<Candidate> candidates_;
    int sel_ = 0;
    int skipped_ = 0;
    bool scanned_ = false;

    void doScan();
    void beginNamePrompt(ScreenStack& stack);
    void doImport(const Candidate& cand, const std::string& name);
};

} // namespace app
