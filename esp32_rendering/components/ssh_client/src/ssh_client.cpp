#include "ssh_client.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "libssh2.h"
#include "libssh2_sftp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

static const char* TAG = "ssh_client";

// SSH task stack size — large enough for crypto operations
static constexpr int SSH_TASK_STACK = 16384;
// SSH task priority on Core 1 (below display at 12, above dashboard at 5)
static constexpr int SSH_TASK_PRIORITY = 10;
static constexpr int SSH_TASK_CORE = 1;

// Send queue for thread-safe keyboard input
static constexpr int SEND_QUEUE_SIZE = 256;

struct SendItem {
    uint8_t data[64];
    size_t len;
};

namespace ssh {

SshClient::SshClient()
    : state_(State::Disconnected),
      data_cb_(nullptr), data_ctx_(nullptr),
      state_cb_(nullptr), state_ctx_(nullptr),
      session_(nullptr), channel_(nullptr),
      socket_fd_(-1), task_handle_(nullptr),
      send_queue_(nullptr) {
    std::memset(&config_, 0, sizeof(config_));
}

SshClient::~SshClient() {
    disconnect();
}

void SshClient::connect(const Config& config) {
    if (state_ != State::Disconnected) {
        disconnect();
    }

    config_ = config;

    // Create send queue
    if (!send_queue_) {
        send_queue_ = xQueueCreate(SEND_QUEUE_SIZE, sizeof(SendItem));
    }

    // Spawn SSH task on Core 1
    xTaskCreatePinnedToCore(
        sshTask, "ssh_client", SSH_TASK_STACK,
        this, SSH_TASK_PRIORITY,
        reinterpret_cast<TaskHandle_t*>(&task_handle_),
        SSH_TASK_CORE
    );
}

void SshClient::send(const uint8_t* data, size_t len) {
    if (!send_queue_ || state_ != State::Connected) return;

    // Split into SendItem-sized chunks if needed
    size_t offset = 0;
    while (offset < len) {
        SendItem item = {};
        item.len = (len - offset > sizeof(item.data)) ? sizeof(item.data) : (len - offset);
        std::memcpy(item.data, data + offset, item.len);
        xQueueSend(static_cast<QueueHandle_t>(send_queue_), &item, pdMS_TO_TICKS(10));
        offset += item.len;
    }
}

void SshClient::resizeTerminal(int cols, int rows) {
    if (!channel_ || state_ != State::Connected) return;
    auto* ch = static_cast<LIBSSH2_CHANNEL*>(channel_);
    libssh2_channel_request_pty_size(ch, cols, rows);
    ESP_LOGI(TAG, "Terminal resized to %dx%d", cols, rows);
}

void SshClient::disconnect() {
    if (channel_) {
        auto* ch = static_cast<LIBSSH2_CHANNEL*>(channel_);
        libssh2_channel_close(ch);
        libssh2_channel_free(ch);
        channel_ = nullptr;
    }
    if (session_) {
        auto* sess = static_cast<LIBSSH2_SESSION*>(session_);
        libssh2_session_disconnect(sess, "Client disconnecting");
        libssh2_session_free(sess);
        session_ = nullptr;
    }
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    if (task_handle_) {
        vTaskDelete(static_cast<TaskHandle_t>(task_handle_));
        task_handle_ = nullptr;
    }
    if (send_queue_) {
        vQueueDelete(static_cast<QueueHandle_t>(send_queue_));
        send_queue_ = nullptr;
    }
    setState(State::Disconnected);
}

// --- Key Management ---

bool SshClient::generateKeypair(char* pubkey_out, size_t pubkey_size) {
    // Generate Ed25519 keypair using libssh2's built-in support
    // Store private key to LittleFS: /littlefs/ssh_ed25519
    // Store public key to LittleFS: /littlefs/ssh_ed25519.pub
    //
    // Ed25519 is the fastest option on ESP32-S3:
    //   Ed25519 sign: 26ms
    //   RSA-2048 sign: 118ms
    //   ECDSA P-256 sign: 67ms

    // TODO: Implement Ed25519 key generation using mbedTLS PSA Crypto API
    // mbedtls_pk_setup() with MBEDTLS_PK_EDDSA, then write to file
    // For now, users can generate keys externally and upload via LittleFS

    ESP_LOGW(TAG, "Ed25519 key generation: upload keys to /littlefs/ssh_ed25519");
    return false;
}

bool SshClient::getPublicKey(char* pubkey_out, size_t pubkey_size) {
    FILE* f = fopen("/littlefs/ssh_ed25519.pub", "r");
    if (!f) return false;
    size_t n = fread(pubkey_out, 1, pubkey_size - 1, f);
    pubkey_out[n] = '\0';
    fclose(f);
    return n > 0;
}

bool SshClient::verifyHostKey() {
    if (!session_) return false;
    auto* sess = static_cast<LIBSSH2_SESSION*>(session_);

    // Get server's host key fingerprint
    const char* fingerprint = libssh2_hostkey_hash(sess, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (!fingerprint) {
        setState(State::Error, "Failed to get host key fingerprint");
        return false;
    }

    // Build storage path based on host
    char path[128];
    snprintf(path, sizeof(path), "/littlefs/known_hosts/%s_%d",
             config_.host, config_.port);

    // Try to read stored fingerprint
    FILE* f = fopen(path, "r");
    if (f) {
        char stored[64] = {};
        fread(stored, 1, sizeof(stored) - 1, f);
        fclose(f);

        // Compare fingerprints (SHA-256 = 32 bytes)
        if (std::memcmp(stored, fingerprint, 32) != 0) {
            setState(State::Error,
                     "HOST KEY CHANGED — possible MITM attack! "
                     "Delete stored key to accept new one.");
            return false;
        }
        ESP_LOGI(TAG, "Host key verified for %s:%d", config_.host, config_.port);
        return true;
    }

    // First connection — TOFU: store the fingerprint
    // Ensure directory exists
    mkdir("/littlefs/known_hosts", 0755);
    f = fopen(path, "w");
    if (f) {
        fwrite(fingerprint, 1, 32, f);
        fclose(f);
        ESP_LOGI(TAG, "Stored host key for %s:%d (TOFU)", config_.host, config_.port);
    }
    return true;
}

// --- SSH Task ---

void SshClient::sshTask(void* param) {
    auto* self = static_cast<SshClient*>(param);

    // Initialize libssh2
    libssh2_init(0);

    if (!self->doConnect()) {
        self->setState(State::Error, "Connection failed");
        libssh2_exit();
        vTaskDelete(nullptr);
        return;
    }

    if (!self->verifyHostKey()) {
        self->disconnect();
        libssh2_exit();
        vTaskDelete(nullptr);
        return;
    }

    if (!self->doAuthenticate()) {
        self->setState(State::Error, "Authentication failed");
        self->disconnect();
        libssh2_exit();
        vTaskDelete(nullptr);
        return;
    }

    // Default terminal: 80×24, will be resized once renderer calculates actual size
    if (!self->openShell(80, 24)) {
        self->setState(State::Error, "Failed to open shell");
        self->disconnect();
        libssh2_exit();
        vTaskDelete(nullptr);
        return;
    }

    self->setState(State::Connected);
    self->ioLoop();

    libssh2_exit();
    self->task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

bool SshClient::doConnect() {
    setState(State::Connecting);

    // Resolve hostname
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", config_.port ? config_.port : 22);

    struct addrinfo* result = nullptr;
    if (getaddrinfo(config_.host, port_str, &hints, &result) != 0 || !result) {
        ESP_LOGE(TAG, "DNS resolution failed for %s", config_.host);
        return false;
    }

    // Create socket
    socket_fd_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_fd_ < 0) {
        freeaddrinfo(result);
        ESP_LOGE(TAG, "Socket creation failed");
        return false;
    }

    // Set TCP_NODELAY for interactive SSH (disable Nagle's algorithm)
    int flag = 1;
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Connect
    if (::connect(socket_fd_, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(socket_fd_);
        socket_fd_ = -1;
        ESP_LOGE(TAG, "TCP connect failed to %s:%s", config_.host, port_str);
        return false;
    }
    freeaddrinfo(result);

    // Create libssh2 session
    auto* sess = libssh2_session_init();
    if (!sess) {
        close(socket_fd_);
        socket_fd_ = -1;
        ESP_LOGE(TAG, "libssh2 session init failed");
        return false;
    }
    session_ = sess;

    // Configure cipher suites for ESP32-S3 hardware
    configureCipherSuites();

    // Set non-blocking mode for the I/O loop
    libssh2_session_set_blocking(sess, 0);

    // Perform SSH handshake (blocking until complete)
    libssh2_session_set_blocking(sess, 1);
    int rc = libssh2_session_handshake(sess, socket_fd_);
    if (rc) {
        ESP_LOGE(TAG, "SSH handshake failed: %d", rc);
        return false;
    }

    // Switch back to non-blocking for I/O
    libssh2_session_set_blocking(sess, 0);

    ESP_LOGI(TAG, "SSH handshake complete with %s:%s", config_.host, port_str);
    return true;
}

void SshClient::configureCipherSuites() {
    auto* sess = static_cast<LIBSSH2_SESSION*>(session_);

    // ESP32-S3 hardware-informed cipher priorities:
    // AES-128-CTR: 7.5 MB/s (hardware accelerated)
    // AES-256-CTR: 7.5 MB/s (hardware accelerated)
    // ChaCha20: 3.3 MB/s (software only)
    // AES-GCM: 1.35 MB/s (GHASH in software — AVOID)
    libssh2_session_method_pref(sess, LIBSSH2_METHOD_CRYPT_CS,
        "aes128-ctr,aes256-ctr,chacha20-poly1305@openssh.com");
    libssh2_session_method_pref(sess, LIBSSH2_METHOD_CRYPT_SC,
        "aes128-ctr,aes256-ctr,chacha20-poly1305@openssh.com");

    // Key exchange: Curve25519 is fastest (15ms, software optimized)
    libssh2_session_method_pref(sess, LIBSSH2_METHOD_KEX,
        "curve25519-sha256,curve25519-sha256@libssh.org,ecdh-sha2-nistp256");

    // Host key: Ed25519 fastest (26ms sign)
    libssh2_session_method_pref(sess, LIBSSH2_METHOD_HOSTKEY,
        "ssh-ed25519,ecdsa-sha2-nistp256,ssh-rsa");

    // MAC: SHA hardware accelerated
    libssh2_session_method_pref(sess, LIBSSH2_METHOD_MAC_CS,
        "hmac-sha2-256,hmac-sha2-512");
    libssh2_session_method_pref(sess, LIBSSH2_METHOD_MAC_SC,
        "hmac-sha2-256,hmac-sha2-512");
}

bool SshClient::doAuthenticate() {
    setState(State::Authenticating);
    auto* sess = static_cast<LIBSSH2_SESSION*>(session_);

    // Make authentication blocking
    libssh2_session_set_blocking(sess, 1);

    // Try key-based auth first if configured
    if (config_.use_key_auth) {
        int rc = libssh2_userauth_publickey_fromfile(
            sess, config_.username,
            "/littlefs/ssh_ed25519.pub",
            "/littlefs/ssh_ed25519",
            nullptr  // passphrase (none for now)
        );
        if (rc == 0) {
            ESP_LOGI(TAG, "Ed25519 key auth successful for %s", config_.username);
            libssh2_session_set_blocking(sess, 0);
            return true;
        }
        ESP_LOGW(TAG, "Key auth failed (rc=%d), trying password", rc);
    }

    // Password auth
    if (config_.password[0]) {
        int rc = libssh2_userauth_password(sess, config_.username, config_.password);
        if (rc == 0) {
            ESP_LOGI(TAG, "Password auth successful for %s", config_.username);
            libssh2_session_set_blocking(sess, 0);
            return true;
        }
        ESP_LOGE(TAG, "Password auth failed: %d", rc);
    }

    libssh2_session_set_blocking(sess, 0);
    return false;
}

bool SshClient::openShell(int cols, int rows) {
    auto* sess = static_cast<LIBSSH2_SESSION*>(session_);
    libssh2_session_set_blocking(sess, 1);

    auto* ch = libssh2_channel_open_session(sess);
    if (!ch) {
        ESP_LOGE(TAG, "Failed to open channel");
        libssh2_session_set_blocking(sess, 0);
        return false;
    }
    channel_ = ch;

    // Request PTY with terminal size
    if (libssh2_channel_request_pty_ex(ch, "xterm-256color", 14,
                                         nullptr, 0, cols, rows, 0, 0)) {
        ESP_LOGE(TAG, "Failed to request PTY");
        libssh2_session_set_blocking(sess, 0);
        return false;
    }

    // Request shell
    if (libssh2_channel_shell(ch)) {
        ESP_LOGE(TAG, "Failed to request shell");
        libssh2_session_set_blocking(sess, 0);
        return false;
    }

    libssh2_session_set_blocking(sess, 0);
    ESP_LOGI(TAG, "Shell opened (%dx%d)", cols, rows);
    return true;
}

void SshClient::ioLoop() {
    auto* ch = static_cast<LIBSSH2_CHANNEL*>(channel_);
    uint8_t recv_buf[4096];
    auto send_q = static_cast<QueueHandle_t>(send_queue_);

    while (state_ == State::Connected) {
        bool activity = false;

        // Read from SSH channel → data callback
        ssize_t n = libssh2_channel_read(ch, reinterpret_cast<char*>(recv_buf),
                                          sizeof(recv_buf));
        if (n > 0) {
            if (data_cb_) {
                data_cb_(recv_buf, n, data_ctx_);
            }
            activity = true;
        } else if (n == LIBSSH2_ERROR_EAGAIN) {
            // No data available, that's fine
        } else if (n < 0) {
            ESP_LOGE(TAG, "Channel read error: %zd", n);
            break;
        }

        // Check for channel EOF
        if (libssh2_channel_eof(ch)) {
            ESP_LOGI(TAG, "Channel EOF received");
            break;
        }

        // Write pending keyboard input → SSH channel
        SendItem item;
        while (xQueueReceive(send_q, &item, 0) == pdTRUE) {
            ssize_t written = 0;
            while (written < static_cast<ssize_t>(item.len)) {
                ssize_t w = libssh2_channel_write(ch,
                    reinterpret_cast<const char*>(item.data + written),
                    item.len - written);
                if (w > 0) {
                    written += w;
                } else if (w == LIBSSH2_ERROR_EAGAIN) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                } else {
                    ESP_LOGE(TAG, "Channel write error: %zd", w);
                    goto exit_loop;
                }
            }
            activity = true;
        }

        // If no activity, yield to other tasks briefly
        if (!activity) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        // Feed watchdog during I/O loop
        esp_task_wdt_reset();
    }

exit_loop:
    setState(State::Disconnected, "SSH session ended");
}

void SshClient::setState(State s, const char* msg) {
    state_ = s;
    if (state_cb_) {
        state_cb_(s, msg, state_ctx_);
    }
}

} // namespace ssh
