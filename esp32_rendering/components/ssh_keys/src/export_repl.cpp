#include "ssh_key_export.hpp"
#include "ssh_keys.hpp"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "esp_console.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "qrcodegen.h"
#include "test_console_response.hpp"
#include "test_console_context.hpp"

namespace ssh_keys {

static test_console::Context* ctx_for_ssh_keys() { return test_console::getContext(); }

static int cmd_ssh_keys_list(int, char**) {
    auto* ctx = ctx_for_ssh_keys();
    if (!ctx) { test_console::err(10, "no context"); return 1; }
    auto& s = ctx->keyStore;
    for (const auto& m : s.keys()) {
        // fp_sha256 is 32 bytes → base64 yields 44 chars + NUL → need 45.
        unsigned char b64[48] = {};
        size_t olen = 0;
        mbedtls_base64_encode(b64, sizeof(b64), &olen, m.fp_sha256.data(), m.fp_sha256.size());
        char fp_head[20] = {};
        size_t take = olen < 16 ? olen : 16;
        std::memcpy(fp_head, b64, take);
        test_console::data("%s %s %s %s",
                           m.id.hex().c_str(),
                           key_type_glyph(m.type, m.rsa_bits),
                           m.name, fp_head);
    }
    test_console::ok("count=%zu", s.keys().size());
    return 0;
}

static int cmd_ssh_keys_pubkey(int argc, char** argv) {
    if (argc != 2) { test_console::err(1, "usage: ssh-keys-pubkey <uuid>"); return 1; }
    auto* ctx = ctx_for_ssh_keys();
    if (!ctx) { test_console::err(10, "no context"); return 1; }
    auto parsed = KeyId::parse(argv[1]);
    if (!parsed) { test_console::err(2, "bad uuid"); return 1; }
    char path[96];
    std::snprintf(path, sizeof(path), "/littlefs/ssh_keys/%s.pub", parsed->hex().c_str());
    FILE* f = std::fopen(path, "rb");
    if (!f) { test_console::err(3, "not found"); return 1; }
    char buf[800] = {};
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    if (n == sizeof(buf) - 1) {
        test_console::err(11, "pubkey line >= %zu B, truncated", sizeof(buf) - 1);
        return 1;
    }
    buf[n] = '\0';
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    test_console::data("%s", buf);
    test_console::ok("%s", "");
    return 0;
}

static int cmd_ssh_keys_delete(int argc, char** argv) {
    if (argc != 2) { test_console::err(1, "usage: ssh-keys-delete <uuid>"); return 1; }
    auto* ctx = ctx_for_ssh_keys();
    if (!ctx) { test_console::err(10, "no context"); return 1; }
    auto parsed = KeyId::parse(argv[1]);
    if (!parsed) { test_console::err(2, "bad uuid"); return 1; }
    // For the REPL path, skip the reference-check (tests may need to force-delete).
    bool rc = ctx->keyStore.delete_key(*parsed, nullptr, nullptr);
    if (!rc) { test_console::err(4, "delete failed"); return 1; }
    test_console::ok("%s", "");
    return 0;
}

static int cmd_ssh_keys_generate(int argc, char** argv) {
    if (argc != 2) { test_console::err(1, "usage: ssh-keys-generate <name>"); return 1; }
    auto* ctx = ctx_for_ssh_keys();
    if (!ctx) { test_console::err(10, "no context"); return 1; }
    KeyId new_id;
    auto rc = generate_ed25519(ctx->keyStore, ctx->wifiMgr, argv[1], 0, new_id);
    switch (rc) {
        case GenerateResult::Ok:
            test_console::ok("%s", new_id.hex().c_str());
            return 0;
        case GenerateResult::WifiDown:       test_console::err(5, "wifi down"); return 1;
        case GenerateResult::LibsshError:    test_console::err(6, "libssh error"); return 1;
        case GenerateResult::StorePushFailed:test_console::err(7, "store push"); return 1;
        case GenerateResult::IndexFull:      test_console::err(8, "index full"); return 1;
    }
    test_console::err(9, "unknown"); return 1;
}

static int cmd_ssh_keys_export(int argc, char** argv) {
    // Alias: identical to pubkey since the test_console protocol already
    // delimits DATA lines via its own sentinels.
    return cmd_ssh_keys_pubkey(argc, argv);
}

static int cmd_ssh_keys_qr_raw(int argc, char** argv) {
    if (argc != 2) { test_console::err(1, "usage: ssh-keys-qr-raw <uuid>"); return 1; }
    auto* ctx = ctx_for_ssh_keys();
    if (!ctx) { test_console::err(10, "no context"); return 1; }
    auto parsed = KeyId::parse(argv[1]);
    if (!parsed) { test_console::err(2, "bad uuid"); return 1; }

    char path[96];
    std::snprintf(path, sizeof(path), "/littlefs/ssh_keys/%s.pub", parsed->hex().c_str());
    FILE* f = std::fopen(path, "rb");
    if (!f) { test_console::err(3, "not found"); return 1; }
    char line[800] = {};
    size_t n = std::fread(line, 1, sizeof(line) - 1, f);
    std::fclose(f);
    if (n == sizeof(line) - 1) {
        test_console::err(11, "pubkey line >= %zu B, truncated", sizeof(line) - 1);
        return 1;
    }
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';

    uint8_t qrb[qrcodegen_BUFFER_LEN_FOR_VERSION(17)];
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(17)];
    if (!qrcodegen_encodeText(line, tmp, qrb, qrcodegen_Ecc_LOW,
                               qrcodegen_VERSION_MIN, 17,
                               qrcodegen_Mask_AUTO, true)) {
        test_console::err(4, "qrcodegen failed"); return 1;
    }
    int modules = qrcodegen_getSize(qrb);
    size_t bytes = (size_t(modules) * modules + 7) / 8;
    std::vector<uint8_t> packed(bytes, 0);
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if (qrcodegen_getModule(qrb, x, y)) {
                size_t bit = size_t(y) * modules + x;
                packed[bit / 8] |= (1u << (bit & 7));
            }
        }
    }
    // V17 max: (85*85+7)/8 = 904 B packed → 1208 B base64. 1280 is a safe ceiling
    // that keeps this frame well under the esp_console task's 8 KB stack.
    unsigned char b64[1280] = {};
    size_t olen = 0;
    int b64_rc = mbedtls_base64_encode(b64, sizeof(b64), &olen, packed.data(), packed.size());
    if (b64_rc != 0) {
        test_console::err(12, "b64 encode (rc=%d, packed=%zu)", b64_rc, packed.size());
        return 1;
    }
    test_console::data("%d %s", modules, reinterpret_cast<const char*>(b64));
    test_console::ok("%s", "");
    return 0;
}

void registerSshKeysCommands() {
    const esp_console_cmd_t cmds[] = {
        {"ssh-keys-list",     "ssh-keys-list — one DATA per key",
         nullptr, cmd_ssh_keys_list, nullptr, nullptr, nullptr},
        {"ssh-keys-pubkey",   "ssh-keys-pubkey <uuid> — DATA=openssh line",
         nullptr, cmd_ssh_keys_pubkey, nullptr, nullptr, nullptr},
        {"ssh-keys-export",   "ssh-keys-export <uuid> — alias of ssh-keys-pubkey",
         nullptr, cmd_ssh_keys_export, nullptr, nullptr, nullptr},
        {"ssh-keys-qr-raw",   "ssh-keys-qr-raw <uuid> — DATA=<modules> <b64-bits>",
         nullptr, cmd_ssh_keys_qr_raw, nullptr, nullptr, nullptr},
        {"ssh-keys-delete",   "ssh-keys-delete <uuid>",
         nullptr, cmd_ssh_keys_delete, nullptr, nullptr, nullptr},
        {"ssh-keys-generate", "ssh-keys-generate <name> — returns uuid",
         nullptr, cmd_ssh_keys_generate, nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace ssh_keys
