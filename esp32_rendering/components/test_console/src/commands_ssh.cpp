#include "test_console_response.hpp"
#include "test_console_context.hpp"

#include "esp_console.h"
#include "esp_log.h"
#include "ssh_client.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

namespace test_console {

// --- ssh-connect <host> <port> <user> <password-b64> [<key_path>] ---
// If a 5th arg is given, we use it as key_path and pass use_key_auth=true.
// Otherwise password-b64 is decoded and we use password auth.
//
// Password is base64-encoded so special characters don't fight the REPL tokenizer.
static int b64_to_buf(const char* in, char* out, size_t out_cap) {
    auto idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    size_t n = 0, len = strlen(in);
    for (size_t i = 0; i + 3 < len && in[i] != '='; i += 4) {
        int v0 = idx(in[i]),   v1 = idx(in[i+1]);
        int v2 = in[i+2] == '=' ? -2 : idx(in[i+2]);
        int v3 = in[i+3] == '=' ? -2 : idx(in[i+3]);
        if (v0 < 0 || v1 < 0) return -1;
        if (n >= out_cap - 1) return -1;
        out[n++] = (v0 << 2) | (v1 >> 4);
        if (v2 == -2) break;
        if (v2 < 0) return -1;
        if (n >= out_cap - 1) return -1;
        out[n++] = ((v1 & 0xF) << 4) | (v2 >> 2);
        if (v3 == -2) break;
        if (v3 < 0) return -1;
        if (n >= out_cap - 1) return -1;
        out[n++] = ((v2 & 0x3) << 6) | v3;
    }
    out[n] = '\0';
    return (int)n;
}

static int cmd_ssh_connect(int argc, char** argv) {
    if (argc != 5 && argc != 6) {
        err(1, "usage: ssh-connect <host> <port> <user> <password-b64|_> [<key_path>]");
        return 1;
    }
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }

    ssh::Config cfg = {};
    strncpy(cfg.host, argv[1], sizeof(cfg.host) - 1);
    cfg.port = (uint16_t)atoi(argv[2]);
    strncpy(cfg.username, argv[3], sizeof(cfg.username) - 1);

    if (argc == 6 && argv[5][0]) {
        cfg.use_key_auth = true;
        strncpy(cfg.key_path, argv[5], sizeof(cfg.key_path) - 1);
    } else if (strcmp(argv[4], "_") != 0) {
        if (b64_to_buf(argv[4], cfg.password, sizeof(cfg.password)) < 0) {
            err(2, "base64 decode failed");
            return 1;
        }
    }

    ctx->sshClient.connect(cfg);
    ok("%s", "");
    return 0;
}

static int cmd_ssh_disconnect(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    ctx->sshClient.disconnect();
    ok("%s", "");
    return 0;
}

static int cmd_ssh_info(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    auto& s = ctx->sshClient;
    ok("cipher_in=%s cipher_out=%s hostkey=%s fp=%s",
       s.lastCipherIn(), s.lastCipherOut(),
       s.lastHostKeyType(), s.lastFingerprint());
    return 0;
}

static int cmd_ssh_last_error(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    ok("%s", ctx->sshClient.lastErrorMessage());
    return 0;
}

static int cmd_ssh_known_hosts_list(int, char**) {
    FILE* f = fopen("/littlefs/known_hosts", "r");
    if (!f) { ok("count=0"); return 0; }
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        // First 3 tokens: host, key-type, blob.
        char host[128] = {}, ktype[32] = {}, blob[768] = {};
        int n = sscanf(line, "%127s %31s %767s", host, ktype, blob);
        if (n < 3) continue;
        char blob_head[17] = {};
        strncpy(blob_head, blob, 16);
        data("%s %s %s", host, ktype, blob_head);
        ++count;
    }
    fclose(f);
    ok("count=%d", count);
    return 0;
}

static int cmd_ssh_known_hosts_erase(int, char**) {
    int rc = unlink("/littlefs/known_hosts");
    if (rc == 0 || errno == ENOENT) {
        ok("%s", "erased");
        return 0;
    }
    // Defensive: legacy pre-swap format kept known_hosts as a directory.
    // If unlink reports EISDIR, recursively remove contents and rmdir it.
    if (errno == EISDIR) {
        DIR* d = opendir("/littlefs/known_hosts");
        if (d) {
            struct dirent* e;
            char path[512];
            while ((e = readdir(d)) != nullptr) {
                if (e->d_name[0] == '.') continue;
                snprintf(path, sizeof(path), "/littlefs/known_hosts/%s", e->d_name);
                unlink(path);
            }
            closedir(d);
        }
        if (rmdir("/littlefs/known_hosts") == 0 || errno == ENOENT) {
            ok("%s", "erased (was directory)");
            return 0;
        }
        err(3, "rmdir: %d", errno);
        return 1;
    }
    err(3, "unlink: %d", errno);
    return 1;
}

void registerSshCommands() {
    const esp_console_cmd_t cmds[] = {
        {"ssh-connect",           "ssh-connect <host> <port> <user> <password-b64|_> [<key_path>]",
         nullptr, cmd_ssh_connect,           nullptr, nullptr, nullptr},
        {"ssh-disconnect",        "ssh-disconnect",
         nullptr, cmd_ssh_disconnect,        nullptr, nullptr, nullptr},
        {"ssh-info",              "ssh-info \xE2\x80\x94 last cipher/hostkey/fingerprint",
         nullptr, cmd_ssh_info,              nullptr, nullptr, nullptr},
        {"ssh-last-error",        "ssh-last-error \xE2\x80\x94 last Error-state message",
         nullptr, cmd_ssh_last_error,        nullptr, nullptr, nullptr},
        {"ssh-known-hosts-list",  "ssh-known-hosts-list \xE2\x80\x94 one DATA per known_hosts entry",
         nullptr, cmd_ssh_known_hosts_list,  nullptr, nullptr, nullptr},
        {"ssh-known-hosts-erase", "ssh-known-hosts-erase \xE2\x80\x94 rm /littlefs/known_hosts",
         nullptr, cmd_ssh_known_hosts_erase, nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
