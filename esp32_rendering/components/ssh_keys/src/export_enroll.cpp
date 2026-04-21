#include "ssh_key_export.hpp"

#include <cstring>
#include <cstdio>

#include "esp_log.h"
#include "ssh_client.hpp"
#include "ssh_keys.hpp"
#include "config_manager.hpp"

static const char* TAG = "ssh_keys";

namespace ssh_keys {

// The canonical shell command. No user-controlled substitution.
// umask + mkdir + chmod sequence is idempotent; `cat >> authorized_keys`
// appends the pubkey line passed as stdin data.
static constexpr const char* ENROLL_CMD =
    "umask 077 && "
    "{ [ -d ~/.ssh ] || mkdir ~/.ssh; } && "
    "chmod 700 ~/.ssh && "
    "cat >> ~/.ssh/authorized_keys && "
    "chmod 600 ~/.ssh/authorized_keys";

static bool load_pubkey_line(const KeyStore& /*store*/, const KeyId& id,
                              char* out, size_t cap) {
    char path[96];
    std::snprintf(path, sizeof(path), "/littlefs/ssh_keys/%s.pub", id.hex().c_str());
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    size_t n = std::fread(out, 1, cap - 1, f);
    std::fclose(f);
    if (n == cap - 1) {
        ESP_LOGE(TAG, "enroll: pubkey >= %zu B, truncated — refusing", cap - 1);
        return false;
    }
    out[n] = '\0';
    // Ensure trailing newline for the append (authorized_keys is line-separated).
    if (n > 0 && out[n-1] != '\n') {
        if (n + 1 < cap) { out[n] = '\n'; out[n+1] = '\0'; }
    }
    return n > 0;
}

EnrollResult enroll_key(KeyStore& store, const KeyId& id,
                        void* config_mgr_opaque,
                        int server_idx, const char* password,
                        char* out_err, size_t out_err_cap) {
    auto* config_mgr = static_cast<sdcard::ConfigManager*>(config_mgr_opaque);
    if (!config_mgr) {
        std::snprintf(out_err, out_err_cap, "no config manager");
        return EnrollResult::ProbeOrPassword;
    }

    const sdcard::ServerRuntime& srv = config_mgr->getServer(server_idx);
    const sdcard::ServerCreds& creds = srv.creds;

    char pubkey_line[800];  // RSA-4096 openssh line ceiling ~780 B + comment + LF
    if (!load_pubkey_line(store, id, pubkey_line, sizeof(pubkey_line))) {
        std::snprintf(out_err, out_err_cap, "pubkey file missing or too large");
        return EnrollResult::ProbeOrPassword;
    }
    bool already_enrolled = false;

    // Helpers: build ssh::Config for a connect
    auto make_key_cfg = [&](ssh::Config& cfg) {
        std::memset(&cfg, 0, sizeof(cfg));
        std::strncpy(cfg.host, creds.host, sizeof(cfg.host) - 1);
        cfg.port = creds.port;
        std::strncpy(cfg.username, creds.username, sizeof(cfg.username) - 1);
        cfg.use_key_auth = true;
        std::strncpy(cfg.ssh_key_id, id.hex().c_str(), sizeof(cfg.ssh_key_id) - 1);
    };
    auto make_pwd_cfg = [&](ssh::Config& cfg) {
        std::memset(&cfg, 0, sizeof(cfg));
        std::strncpy(cfg.host, creds.host, sizeof(cfg.host) - 1);
        cfg.port = creds.port;
        std::strncpy(cfg.username, creds.username, sizeof(cfg.username) - 1);
        cfg.use_key_auth = false;
        std::strncpy(cfg.password, password, sizeof(cfg.password) - 1);
    };

    ssh::SshClient helper;
    int exit_code = -1;

    // Step 2 (spec §4.1): probe key-auth — if it works, no upload needed.
    {
        ssh::Config cfg;
        make_key_cfg(cfg);
        char dummy_err[64] = {};
        int rc = helper.execOneshot(cfg, "true", nullptr, 0, 15000,
                                     &exit_code, dummy_err, sizeof(dummy_err));
        if (rc == 0 && exit_code == 0) {
            ESP_LOGI(TAG, "enroll: probe succeeded — already enrolled");
            already_enrolled = true;
        }
    }

    if (!already_enrolled) {
        // Steps 3+4 combined: connect with password + upload the pubkey line via
        // stdin stream to the fixed shell command.
        {
            ssh::Config cfg;
            make_pwd_cfg(cfg);
            char ssh_err[128] = {};
            int rc = helper.execOneshot(cfg, ENROLL_CMD,
                                         reinterpret_cast<const uint8_t*>(pubkey_line),
                                         std::strlen(pubkey_line),
                                         20000, &exit_code,
                                         ssh_err, sizeof(ssh_err));
            // Spec §4.1: zero the password after step 4 regardless of outcome.
            std::memset(cfg.password, 0, sizeof(cfg.password));
            if (rc != 0) {
                std::snprintf(out_err, out_err_cap, "probe-or-password: %s", ssh_err);
                return EnrollResult::ProbeOrPassword;
            }
            if (exit_code != 0) {
                std::snprintf(out_err, out_err_cap, "upload exit=%d", exit_code);
                return EnrollResult::Upload;
            }
        }

        // Step 5: verify with key auth in a FRESH session (RFC 4252 disallows
        // re-auth inside an authenticated session).
        {
            ssh::Config cfg;
            make_key_cfg(cfg);
            char ssh_err[128] = {};
            int rc = helper.execOneshot(cfg, "true", nullptr, 0, 15000,
                                         &exit_code, ssh_err, sizeof(ssh_err));
            if (rc != 0 || exit_code != 0) {
                std::snprintf(out_err, out_err_cap, "verify: %s", ssh_err);
                return EnrollResult::Verify;
            }
        }
    }

    // Step 6: flip server record to key auth.
    {
        sdcard::ServerCreds new_creds = creds;
        new_creds.use_key_auth = true;
        std::strncpy(new_creds.ssh_key_id, id.hex().c_str(), sizeof(new_creds.ssh_key_id) - 1);
        std::memset(new_creds.password, 0, sizeof(new_creds.password));
        if (config_mgr->upsertServer(new_creds, server_idx) < 0) {
            std::snprintf(out_err, out_err_cap, "flip: NVS write failed");
            return EnrollResult::FlipFailed;
        }
    }
    ESP_LOGI(TAG, "enroll: success for server %d%s", server_idx,
             already_enrolled ? " (already enrolled, local record updated)" : "");
    return already_enrolled ? EnrollResult::AlreadyEnrolled : EnrollResult::Ok;
}

} // namespace ssh_keys
