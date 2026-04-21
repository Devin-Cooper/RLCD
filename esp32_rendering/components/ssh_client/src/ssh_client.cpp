#include "ssh_client.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "libssh/libssh.h"
#include "libssh_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

static const char* TAG = "ssh_client";

static constexpr int SSH_TASK_STACK = 16384;
static constexpr int SSH_TASK_PRIORITY = 10;
static constexpr int SSH_TASK_CORE = 1;
static constexpr int SEND_QUEUE_SIZE = 256;
static constexpr int KEEPALIVE_INTERVAL_SEC = 30;
static constexpr int DISCONNECT_WATCHDOG_MS = 1000;
static constexpr const char* KNOWN_HOSTS_PATH = "/littlefs/known_hosts";
static constexpr const char* KNOWN_HOSTS_NVS_NS = "ssh_host";
static constexpr const char* KNOWN_HOSTS_NVS_KEY = "migrated_v2";

struct SendItem {
    uint8_t data[16];
    size_t len;
};

namespace ssh {

static bool is_valid_hostname(const char* s) {
    if (!s) return false;
    size_t n = 0;
    while (s[n] != '\0') ++n;
    if (n == 0 || n > 253) return false;
    if (s[0] == '-' || s[0] == '.') return false;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        bool ok =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

// One-shot migration: if a pre-swap known_hosts exists (file OR directory),
// remove it. libssh uses OpenSSH-native format which our previous code did
// not produce. Guarded by NVS sentinel so it runs exactly once.
static void run_known_hosts_migration_once() {
    nvs_handle_t h;
    if (nvs_open(KNOWN_HOSTS_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    uint8_t sentinel = 0;
    nvs_get_u8(h, KNOWN_HOSTS_NVS_KEY, &sentinel);
    if (sentinel == 1) {
        nvs_close(h);
        return;
    }

    struct stat st;
    if (stat(KNOWN_HOSTS_PATH, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            // Pre-swap firmware stored per-host files in /littlefs/known_hosts/
            // as a directory — the format is incompatible with libssh's flat-file
            // known_hosts. Erase the directory and its contents; TOFU will
            // repopulate as a flat file on next connect.
            DIR* d = opendir(KNOWN_HOSTS_PATH);
            if (d) {
                struct dirent* e;
                // 512 > strlen(KNOWN_HOSTS_PATH) + 1 + max d_name (255 per POSIX)
                // — required to dodge -Werror=format-truncation on GCC 14.
                char path[512];
                while ((e = readdir(d)) != nullptr) {
                    if (e->d_name[0] == '.') continue;
                    snprintf(path, sizeof(path), "%s/%s", KNOWN_HOSTS_PATH, e->d_name);
                    unlink(path);
                }
                closedir(d);
                rmdir(KNOWN_HOSTS_PATH);
            }
        } else {
            unlink(KNOWN_HOSTS_PATH);
        }
        ESP_LOGI(TAG, "Migrated away from pre-swap known_hosts format");
    }

    nvs_set_u8(h, KNOWN_HOSTS_NVS_KEY, 1);
    nvs_commit(h);
    nvs_close(h);
}

SshClient::SshClient()
    : state_(State::Disconnected),
      data_cb_(nullptr), data_ctx_(nullptr),
      state_cb_(nullptr), state_ctx_(nullptr),
      session_(nullptr), channel_(nullptr),
      socket_fd_(-1), task_handle_(nullptr),
      send_queue_(nullptr),
      shutdown_requested_(false),
      task_exited_(false) {
    std::memset(&config_, 0, sizeof(config_));
}

SshClient::~SshClient() {
    disconnect();
}

void SshClient::connect(const Config& config) {
    if (state_.load(std::memory_order_acquire) != State::Disconnected) {
        disconnect();
    }

    libssh_port_init();
    run_known_hosts_migration_once();

    config_ = config;
    shutdown_requested_ = false;
    task_exited_ = false;
    std::memset(last_cipher_in_, 0, sizeof(last_cipher_in_));
    std::memset(last_cipher_out_, 0, sizeof(last_cipher_out_));
    std::memset(last_hostkey_type_, 0, sizeof(last_hostkey_type_));
    std::memset(last_fingerprint_, 0, sizeof(last_fingerprint_));
    std::memset(last_error_message_, 0, sizeof(last_error_message_));

    if (!send_queue_) {
        send_queue_ = xQueueCreate(SEND_QUEUE_SIZE, sizeof(SendItem));
    }

    xTaskCreatePinnedToCore(
        sshTask, "ssh_client", SSH_TASK_STACK,
        this, SSH_TASK_PRIORITY,
        reinterpret_cast<TaskHandle_t*>(&task_handle_),
        SSH_TASK_CORE
    );
}

void SshClient::send(const uint8_t* data, size_t len) {
    if (!send_queue_ || state_.load(std::memory_order_acquire) != State::Connected) return;
    size_t offset = 0;
    while (offset < len) {
        SendItem item = {};
        item.len = (len - offset > sizeof(item.data)) ? sizeof(item.data) : (len - offset);
        std::memcpy(item.data, data + offset, item.len);
        xQueueSend(static_cast<QueueHandle_t>(send_queue_), &item, pdMS_TO_TICKS(10));
        offset += item.len;
    }
}

// Stubbed — real body in Task 3.4 once channel ops are wired.
void SshClient::resizeTerminal(int /*cols*/, int /*rows*/) { /* TODO Task 3.4 */ }

void SshClient::teardown() {
    // Idempotent: each pointer is nulled after free so re-entry is safe.
    if (channel_) {
        auto* ch = static_cast<ssh_channel>(channel_);
        ssh_channel_close(ch);
        ssh_channel_free(ch);
        channel_ = nullptr;
    }
    if (session_) {
        // libssh 0.11's ssh_free() calls close() on the fd we handed over
        // via SSH_OPTIONS_FD. Do NOT double-close — null socket_fd_ before
        // ssh_free so the else-branch below is skipped.
        auto* sess = static_cast<ssh_session>(session_);
        ssh_disconnect(sess);
        ssh_free(sess);
        session_ = nullptr;
        socket_fd_ = -1;
    } else if (socket_fd_ >= 0) {
        // Session was never created (or creation failed); we still own the fd.
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

void SshClient::fail(const char* where) {
    const char* detail = "no session";
    if (session_) detail = ssh_get_error(static_cast<ssh_session>(session_));
    char msg[128];
    snprintf(msg, sizeof(msg), "%s: %s", where, detail ? detail : "(null)");
    ESP_LOGW(TAG, "%s", msg);
    strncpy(last_error_message_, msg, sizeof(last_error_message_) - 1);
    setState(State::Error, msg);
    teardown();
}

void SshClient::disconnect() {
    shutdown_requested_ = true;

    // Spin on task_exited_ atomic rather than task_handle_ — the task sets
    // task_exited_ before it begins teardown/vTaskDelete, so this is race-free.
    if (task_handle_) {
        int ticks = DISCONNECT_WATCHDOG_MS / 20;
        for (int i = 0; i < ticks && !task_exited_.load(std::memory_order_acquire); ++i) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (!task_exited_.load(std::memory_order_acquire)) {
            ESP_LOGW(TAG, "Disconnect watchdog: force-killing ssh task");
            vTaskDelete(static_cast<TaskHandle_t>(task_handle_));
            task_handle_ = nullptr;
            teardown();  // task didn't get to clean up; do it ourselves
        }
        task_handle_ = nullptr;
    }

    if (send_queue_) {
        vQueueDelete(static_cast<QueueHandle_t>(send_queue_));
        send_queue_ = nullptr;
    }
    setState(State::Disconnected);
}

bool SshClient::verifyHostKey() {
    auto* sess = static_cast<ssh_session>(session_);
    if (!sess) { fail("verifyHostKey: no session"); return false; }

    if (!is_valid_hostname(config_.host)) {
        ESP_LOGE(TAG, "Rejected invalid SSH host '%s'", config_.host);
        strncpy(last_error_message_, "Invalid SSH host", sizeof(last_error_message_) - 1);
        setState(State::Error, "Invalid SSH host — refusing to connect");
        teardown();
        return false;
    }

    ssh_key pk = nullptr;
    if (ssh_get_server_publickey(sess, &pk) != SSH_OK) {
        fail("get_server_publickey");
        return false;
    }
    unsigned char* hash = nullptr;
    size_t hash_len = 0;
    if (ssh_get_publickey_hash(pk, SSH_PUBLICKEY_HASH_SHA256, &hash, &hash_len) != 0) {
        ssh_key_free(pk);
        fail("get_publickey_hash");
        return false;
    }
    char* hex = ssh_get_hexa(hash, hash_len);
    if (hex) {
        snprintf(last_fingerprint_, sizeof(last_fingerprint_), "SHA256:%s", hex);
        ESP_LOGI(TAG, "Server fingerprint %s", last_fingerprint_);
        ssh_string_free_char(hex);
    }
    ssh_clean_pubkey_hash(&hash);
    const char* type_str = ssh_key_type_to_char(ssh_key_type(pk));
    if (type_str) strncpy(last_hostkey_type_, type_str, sizeof(last_hostkey_type_) - 1);
    ssh_key_free(pk);

    int known = ssh_session_is_known_server(sess);
    switch (known) {
    case SSH_KNOWN_HOSTS_OK:
        return true;
    case SSH_KNOWN_HOSTS_UNKNOWN:
        if (ssh_session_update_known_hosts(sess) != SSH_OK) {
            fail("update_known_hosts");
            return false;
        }
        ESP_LOGI(TAG, "TOFU: stored host key for %s:%d", config_.host, config_.port);
        return true;
    case SSH_KNOWN_HOSTS_CHANGED: {
        const char* msg = "HOST KEY CHANGED — possible MITM. Delete known_hosts to accept.";
        strncpy(last_error_message_, msg, sizeof(last_error_message_) - 1);
        setState(State::Error, msg);
        teardown();
        return false;
    }
    case SSH_KNOWN_HOSTS_OTHER: {
        const char* msg = "Server key type changed — delete known_hosts to accept.";
        strncpy(last_error_message_, msg, sizeof(last_error_message_) - 1);
        setState(State::Error, msg);
        teardown();
        return false;
    }
    case SSH_KNOWN_HOSTS_ERROR:
    default: {
        const char* msg = "known_hosts unreadable — see logs.";
        strncpy(last_error_message_, msg, sizeof(last_error_message_) - 1);
        setState(State::Error, msg);
        ESP_LOGE(TAG, "ssh_session_is_known_server: %s", ssh_get_error(sess));
        teardown();
        return false;
    }
    }
}

bool SshClient::doAuthenticate() {
    // Stubbed — real body in Task 3.3.
    setState(State::Error, "doAuthenticate not yet implemented");
    return false;
}

bool SshClient::openShell(int /*cols*/, int /*rows*/) {
    // Stubbed — real body in Task 3.4.
    setState(State::Error, "openShell not yet implemented");
    return false;
}

void SshClient::ioLoop() { /* TODO Task 3.4 */ }

void SshClient::configureCipherSuites() { /* TODO Task 3.5 */ }

bool SshClient::doConnect() {
    setState(State::Connecting);

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", config_.port ? config_.port : 22);

    struct addrinfo* result = nullptr;
    if (getaddrinfo(config_.host, port_str, &hints, &result) != 0 || !result) {
        char msg[128];
        snprintf(msg, sizeof(msg), "DNS resolution failed for %s", config_.host);
        strncpy(last_error_message_, msg, sizeof(last_error_message_) - 1);
        setState(State::Error, msg);
        return false;
    }

    socket_fd_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_fd_ < 0) {
        freeaddrinfo(result);
        strncpy(last_error_message_, "socket() failed", sizeof(last_error_message_) - 1);
        setState(State::Error, "socket() failed");
        return false;
    }

    int flag = 1;
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    int keepalive = 1, idle = 30, interval = 5, count = 3;
    setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));

    if (::connect(socket_fd_, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(socket_fd_);
        socket_fd_ = -1;
        char msg[128];
        snprintf(msg, sizeof(msg), "TCP connect to %s:%s: %s",
                 config_.host, port_str, strerror(errno));
        strncpy(last_error_message_, msg, sizeof(last_error_message_) - 1);
        setState(State::Error, msg);
        return false;
    }
    freeaddrinfo(result);

    auto sess = ssh_new();
    if (!sess) {
        close(socket_fd_);
        socket_fd_ = -1;
        strncpy(last_error_message_, "ssh_new() failed", sizeof(last_error_message_) - 1);
        setState(State::Error, "ssh_new() failed");
        return false;
    }
    session_ = sess;

    ssh_options_set(sess, SSH_OPTIONS_HOST, config_.host);
    int port = config_.port ? config_.port : 22;
    ssh_options_set(sess, SSH_OPTIONS_PORT, &port);
    ssh_options_set(sess, SSH_OPTIONS_USER, config_.username);
    ssh_options_set(sess, SSH_OPTIONS_FD, &socket_fd_);
    ssh_options_set(sess, SSH_OPTIONS_KNOWNHOSTS, KNOWN_HOSTS_PATH);
    int log_level = CONFIG_LIBSSH_LOG_LEVEL;
    ssh_options_set(sess, SSH_OPTIONS_LOG_VERBOSITY, &log_level);

    configureCipherSuites();

    ssh_set_blocking(sess, 1);
    if (ssh_connect(sess) != SSH_OK) {
        fail("ssh_connect");
        return false;
    }

    // libssh 0.11.4 has no ssh_set_keepalive_options(); TCP-level keepalive
    // is already active via setsockopt(SO_KEEPALIVE,...) above. Application-
    // level keepalive would require periodic ssh_send_keepalive() pokes from
    // the ioLoop (Task 3.4) — deferred. KEEPALIVE_INTERVAL_SEC stays on the
    // file to document the intended cadence.
    (void)KEEPALIVE_INTERVAL_SEC;

    ESP_LOGI(TAG, "SSH handshake complete with %s:%s", config_.host, port_str);
    return true;
}

void SshClient::sshTask(void* param) {
    auto* self = static_cast<SshClient*>(param);
    esp_task_wdt_add(nullptr);

    bool connected = self->doConnect() &&
                     self->verifyHostKey() &&
                     self->doAuthenticate() &&
                     self->openShell(80, 24);

    if (connected) {
        self->setState(State::Connected);
        // Connection-info snapshot for ssh-info consumers — populated in Task 3.5.
        self->ioLoop();
    }
    // else: fail() already emitted Error and torn down.

    // Mark exit FIRST, THEN teardown, THEN delete self. disconnect()'s
    // spin-loop observes task_exited_ before we touch any libssh state.
    self->task_exited_.store(true, std::memory_order_release);
    self->teardown();
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
}

void SshClient::setState(State s, const char* msg) {
    state_.store(s, std::memory_order_release);
    if (state_cb_) {
        state_cb_(s, msg, state_ctx_);
    }
}

} // namespace ssh
