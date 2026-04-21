#pragma once

#include <cstdint>
#include <cstring>
#include <atomic>

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
    bool use_key_auth;      // true → authenticate via ssh_key_id lookup in KeyStore; false → password
    char ssh_key_id[33];    // 32 hex chars + null; KeyStore::path_for resolves to PEM path
};

using DataCallback = void(*)(const uint8_t* data, size_t len, void* ctx);
using StateCallback = void(*)(State state, const char* message, void* ctx);

/// SSH client for interactive terminal sessions.
///
/// Uses libssh 0.11.4 (vendored from ewpa/LibSSH-ESP32 as components/libssh/)
/// with ESP-IDF's mbedTLS backend. Runs its own FreeRTOS task pinned to
/// Core 1 for non-blocking I/O. Single-session — libssh is not thread-safe
/// across concurrent sessions without ssh_threads_set_callbacks.
///
/// Cipher-suite preferences (ESP32-S3 hardware-informed; numbers re-measured
/// post-swap — see README):
///   KEX:    curve25519-sha256, ecdh-sha2-nistp256
///   Host:   ssh-ed25519, ecdsa-sha2-nistp256, rsa-sha2-512, rsa-sha2-256
///           (ssh-rsa / SHA-1 signatures excluded)
///   Cipher: aes128-ctr, aes256-ctr, chacha20-poly1305@openssh.com
///           AVOID aes-gcm (GHASH is software)
///   MAC:    hmac-sha2-256, hmac-sha2-512
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

    /// Verify server host key (TOFU — Trust On First Use).
    /// Returns false if key changed from stored value.
    /// On first connection (no stored key), stores the key.
    /// If mismatch: calls state callback with Error + warning message.
    bool verifyHostKey();

    /// One-shot exec channel for SSH key enrollment.
    ///
    /// Opens a session using Config.use_key_auth + Config.password OR Config.ssh_key_id,
    /// runs a fixed command, optionally streams stdin bytes, closes stdin,
    /// waits up to timeout_ms for completion, returns exit code via out_exit_code.
    ///
    /// Synchronous: returns only after the session is torn down. Runs inline on
    /// the calling task.
    ///
    /// Returns 0 on clean exit (out_exit_code populated; 0 = remote success);
    /// returns -1 on SSH-level failure (out_err filled, out_exit_code untouched).
    ///
    /// This API is separate from connect()/send() and does NOT touch the
    /// interactive-shell state machine. Safe to call in any order relative
    /// to connect()/disconnect() — it uses its own local session handles.
    int execOneshot(const Config& cfg,
                    const char* command,
                    const uint8_t* stdin_bytes, size_t stdin_len,
                    int timeout_ms,
                    int* out_exit_code,
                    char* out_err, size_t out_err_cap);

    // Snapshots populated at State::Connected; empty strings otherwise.
    // Consumed by test_console ssh-info.
    const char* lastCipherIn()  const { return last_cipher_in_; }
    const char* lastCipherOut() const { return last_cipher_out_; }
    const char* lastHostKeyType() const { return last_hostkey_type_; }
    const char* lastFingerprint() const { return last_fingerprint_; }
    /// Most recent Error-state message (or empty string if never errored).
    const char* lastErrorMessage() const { return last_error_message_; }

private:
    std::atomic<State> state_;
    DataCallback data_cb_;
    void* data_ctx_;
    StateCallback state_cb_;
    void* state_ctx_;

    // libssh handles (opaque — cast in .cpp)
    void* session_;
    void* channel_;
    int socket_fd_;

    // FreeRTOS task handle
    void* task_handle_;

    // Send queue for thread-safe keyboard input
    void* send_queue_;

    // Graceful shutdown flag — set by disconnect(), checked by ioLoop()
    std::atomic<bool> shutdown_requested_;

    // Set by the task just before it reaches its terminal teardown point.
    // disconnect() spins on this instead of the non-atomic task_handle_ to
    // avoid racing teardown() with the task's own exit cleanup.
    std::atomic<bool> task_exited_;

    Config config_;

    // Connection-info snapshot for test_console ssh-info. Updated at Connected.
    char last_cipher_in_[32]    = {};
    char last_cipher_out_[32]   = {};
    char last_hostkey_type_[16] = {};
    char last_fingerprint_[100] = {};
    char last_error_message_[128] = {};

    void setState(State s, const char* msg = "");
    void fail(const char* where);
    void teardown();
    static void sshTask(void* param);
    bool doConnect();
    bool doAuthenticate();
    bool openShell(int cols, int rows);
    void ioLoop();
    void configureCipherSuites();
};

} // namespace ssh

/// Install the callback that resolves ssh_key_id → PEM path on disk.
/// Called once at boot from main after KeyStore is constructed.
/// The callback must be set BEFORE any connect() call happens.
/// Declared at global scope with C linkage so the name doesn't land in
/// namespace ssh{}; the symbol is what main.cpp invokes after constructing
/// the KeyStore.
extern "C" void ssh_client_set_resolve_key_path(
    bool (*fn)(const char* id, char* out, size_t cap));
