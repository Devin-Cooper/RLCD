#pragma once

#include "ssh_keys.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wifi { class WifiManager; }

namespace ssh_keys {

/// Returns "SHA256:<43-char-b64>" matching `ssh-keygen -lf` output for the
/// passed 32-byte SHA-256 digest. Buffer `out` must have at least 53 bytes
/// (7 for "SHA256:" prefix + 43 for unpadded base64 + 1 NUL + slack).
/// Returns true on success. Uses mbedtls base64 on firmware. Declared here
/// (firmware-facing header) so host-only tests linking against
/// key_codec_host.cpp don't need a duplicate implementation.
bool fp_sha256_b64(const std::array<uint8_t, 32>& fp, char* out, size_t cap);

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

/// QR render: rasterizes the pubkey line as a QR code onto a framebuffer.
/// `framebuffer_opaque` must be an `onebit::IFramebuffer*`; type is erased
/// here so this header stays free of 1-bit display types. Pixel
/// (origin_x, origin_y) is the TOP-LEFT of the QR module grid; the caller
/// provides surrounding whitespace (quiet zone).
///
/// Returns the pixel scale used on success (e.g. 6 for V6 @ 294 px).
/// Returns 0 on any failure; every failure path logs via
/// ESP_LOGE("ssh_keys", ...). Failure modes: key not in store, cached
/// `.pub` unreadable, pubkey line too large for the buffer, nayuki
/// encoder refuses the payload, or computed scale is 0.
int render_qr_to_framebuffer(KeyStore& store, const KeyId& id,
                             void* framebuffer_opaque, int origin_x, int origin_y);

/// Pure helper, exposed for host unit test.
int select_qr_scale(int modules);

} // namespace ssh_keys
