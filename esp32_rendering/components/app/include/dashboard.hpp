#pragma once

#include "ssh_client.hpp"
#include "settings.hpp"
#include "config_manager.hpp"
#include "dashboard_snapshot.hpp"
#include <cstdint>

namespace app {

/// Dashboard data model: runs SSH commands periodically and parses output
/// into a `DashboardSnapshot`. Pure data — rendering is performed by
/// `dashboard_cards.cpp` against the snapshot.
///
/// Reads config from /littlefs/dashboard.cfg if present.
/// Falls back to built-in defaults: loadavg, free, df, uptime, sensors, screen -ls.
class Dashboard {
public:
    Dashboard();

    /// Initialize dashboard. Loads config from LittleFS or uses defaults.
    void init(const Settings& settings);

    /// Tick the dashboard timer. Runs commands if interval has elapsed.
    /// @param ssh SSH client to send commands through.
    /// @param now_ms Current time in milliseconds (esp_timer_get_time()/1000).
    void update(ssh::SshClient& ssh, int64_t now_ms);

    /// Feed SSH response data for parsing.
    void feedData(const uint8_t* data, size_t len);

    /// Update the refresh interval.
    void setInterval(uint16_t interval_ms) { interval_ms_ = interval_ms; }

    /// Update dashboard commands from a server config.
    /// Replaces current commands with the provided ones.
    void updateCommands(const sdcard::DashboardCommand* cmds, int count);

    /// Set the server name displayed in the title bar.
    void setServerName(const char* name);

    const DashboardSnapshot& snapshot() const { return snapshot_; }
    int                      commandCount() const { return command_count_; }

    // Read-only view of one configured command (label/output/etc).
    // Out-of-range `i` returns a CommandView with valid=false and empty-
    // string (not nullptr) text fields, so callers can safely pass the
    // result through to text drawing without null-checking the strings.
    struct CommandView {
        const char* label;
        const char* command;
        const char* output;
        int         output_len;
        bool        valid;
        bool        overflowed;
    };
    CommandView commandAt(int i) const;

private:
    /// Maximum number of dashboard commands
    static constexpr int MAX_COMMANDS = 8;
    /// Maximum output buffer per command (raised from 256 for commands like
    /// `df -h` with many filesystems — Spec 04 Bug 2).
    static constexpr int MAX_OUTPUT_LEN = 1024;
    /// Milliseconds of silence after buffer overflow before advancing.
    static constexpr int64_t OVERFLOW_TIMEOUT_MS = 5000;
    /// Maximum label length
    static constexpr int MAX_LABEL_LEN = 24;
    /// History length for sparkline graphs
    static constexpr int HISTORY_LEN = 60;

    struct DashboardCommand {
        char label[MAX_LABEL_LEN];
        char command[128];
        char output[MAX_OUTPUT_LEN];
        int output_len;
        bool valid;
        // Spec 04 Bug 2: overflow tracking. Once output fills capacity-1,
        // feedChunk's sentinel scan cannot match; we advance on timeout.
        bool overflowed;
        int64_t first_byte_ms;
    };

    DashboardCommand commands_[MAX_COMMANDS];
    int command_count_;
    int current_command_;       // Index of command currently being collected
    bool collecting_;           // True while receiving output for a command
    bool need_send_next_;       // True when sentinel detected, next command needs sending
    bool skip_echo_;            // True while skipping the echoed command line

    uint16_t interval_ms_;
    int64_t last_update_ms_;

    // --- Parsed metric values ---
    DashboardSnapshot snapshot_;

    /// Parse /littlefs/dashboard.cfg. Returns false if file not found.
    bool loadConfig();

    /// Load built-in default commands.
    void loadDefaults();

    /// Send the next command via SSH.
    void sendNextCommand(ssh::SshClient& ssh);

    /// Parse command outputs into structured fields after sentinel detection.
    void parseOutputs();
};

} // namespace app
