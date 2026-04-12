#pragma once

#include <cstdint>
#include <cstring>

namespace ssh {

enum class State {
    Disconnected,
    Connecting,
    Authenticating,
    Connected,
    Error,
};

struct Config {
    char host[64];
    uint16_t port;          // default 22
    char username[32];
    char password[64];      // empty if using key auth
    bool use_key_auth;      // Ed25519 key auth (preferred — 26ms vs 118ms RSA)
};

using DataCallback = void(*)(const uint8_t* data, size_t len, void* ctx);
using StateCallback = void(*)(State state, const char* message, void* ctx);

/// SSH client for interactive terminal sessions.
///
/// Uses skuodi/libssh2_esp (libssh2 v1.11.1 with mbedTLS backend).
/// Runs its own FreeRTOS task pinned to Core 1 for non-blocking I/O.
///
/// Cipher suite priorities (ESP32-S3 hardware-informed):
///   KEX:    curve25519-sha256 (15ms) > ecdh-sha2-nistp256 (62ms)
///   Host:   ssh-ed25519 (26ms sign) > ecdsa-sha2-nistp256 (67ms)
///   Cipher: aes128-ctr (7.5 MB/s hw) > aes256-ctr > chacha20-poly1305 (3.3 MB/s)
///           AVOID aes128-gcm / aes256-gcm (1.35 MB/s — GHASH in software)
///   MAC:    hmac-sha2-256 (26 MB/s) > hmac-sha2-512 (28.6 MB/s)
class SshClient {
public:
    SshClient();
    ~SshClient();

    SshClient(const SshClient&) = delete;
    SshClient& operator=(const SshClient&) = delete;

    /// Set callbacks before connecting.
    void onData(DataCallback cb, void* ctx = nullptr) {
        data_cb_ = cb;
        data_ctx_ = ctx;
    }
    void onStateChange(StateCallback cb, void* ctx = nullptr) {
        state_cb_ = cb;
        state_ctx_ = ctx;
    }

    /// Connect and authenticate. Spawns a background task on Core 1.
    void connect(const Config& config);

    /// Send data (keystrokes) to the SSH channel.
    void send(const uint8_t* data, size_t len);

    /// Request terminal size change (SIGWINCH).
    void resizeTerminal(int cols, int rows);

    /// Disconnect and clean up.
    void disconnect();

    /// Get current state.
    State state() const { return state_; }

    /// Generate Ed25519 keypair, store in LittleFS. Returns public key string.
    /// Ed25519 is preferred: 26ms sign vs 118ms RSA-2048, 67ms ECDSA P-256.
    static bool generateKeypair(char* pubkey_out, size_t pubkey_size);

    /// Load public key from storage.
    static bool getPublicKey(char* pubkey_out, size_t pubkey_size);

    /// Verify server host key (TOFU — Trust On First Use).
    /// Returns false if key changed from stored value.
    /// On first connection (no stored key), stores the key.
    /// If mismatch: calls state callback with Error + warning message.
    bool verifyHostKey();

private:
    State state_;
    DataCallback data_cb_;
    void* data_ctx_;
    StateCallback state_cb_;
    void* state_ctx_;

    // libssh2 handles (opaque — cast in .cpp)
    void* session_;
    void* channel_;
    int socket_fd_;

    // FreeRTOS task handle
    void* task_handle_;

    // Send queue for thread-safe keyboard input
    void* send_queue_;

    Config config_;

    void setState(State s, const char* msg = "");
    static void sshTask(void* param);
    bool doConnect();
    bool doAuthenticate();
    bool openShell(int cols, int rows);
    void ioLoop();
    void configureCipherSuites();
};

} // namespace ssh
