#pragma once

#include "ssh_keys.hpp"

#include <cstddef>
#include <cstdint>

namespace wifi { class WifiManager; }

namespace ssh_keys {

enum class GenerateResult : uint8_t {
    Ok = 0,
    WifiDown,
    LibsshError,
    StorePushFailed,
    IndexFull,
};

/// Wi-Fi-gated Ed25519 generate. Caller must hand in a valid WifiManager
/// reference; the function refuses if state != Connected. `now_utc` comes
/// from PCF85063; pass 0 if RTC isn't yet synced.
GenerateResult generate_ed25519(KeyStore& store, wifi::WifiManager& wifi,
                                const char* name, uint64_t now_utc,
                                KeyId& out_id);

/// SSH enrollment (try-auth-first → stdin-stream → verify → flip server).
/// Server lookup and password plumbing happen inside enroll_screen; this
/// free function performs the network work. `server_idx` is the sdcard_config
/// server index; on success the ServerCreds are flipped to use the key.
///
/// `out_err` is filled with a one-line message on failure; caller displays.
enum class EnrollResult : uint8_t {
    Ok = 0,
    AlreadyEnrolled,    // probe succeeded, short-circuit; ServerCreds flipped
    ProbeOrPassword,
    Upload,
    Verify,
    FlipFailed,
};

class ConfigManager;  // forward; real include in .cpp
EnrollResult enroll_key(KeyStore& store, const KeyId& id,
                        /*sdcard::ConfigManager&*/ void* config_mgr,
                        int server_idx, const char* password,
                        char* out_err, size_t out_err_cap);

/// SD export: writes the pubkey line to /sdcard/export/<name>.pub atomically.
bool export_sd(KeyStore& store, const KeyId& id,
               char* out_err, size_t out_err_cap);

/// Text wrap for the 8x12 font (23 chars/line); returns lines as one string
/// with embedded '\n' and fixed-width column indent inside the b64 blob.
/// For rendering on SshKeyPubkeyTextScreen.
std::string wrapped_pubkey_line(KeyStore& store, const KeyId& id);

/// QR render: writes a QR matrix for the pubkey line into the framebuffer.
/// Returns the module size used (0 on "too large"). Scale auto-selected
/// via select_qr_scale.
int render_qr_to_framebuffer(KeyStore& store, const KeyId& id,
                             void* framebuffer, int origin_x, int origin_y);

/// Pure helper, exposed for host unit test.
int select_qr_scale(int modules);

} // namespace ssh_keys
