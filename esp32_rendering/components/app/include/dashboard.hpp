#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "ssh_client.hpp"
#include "settings.hpp"
#include "config_manager.hpp"
#include <cstdint>

namespace app {

/// Dashboard mode: runs SSH commands periodically, parses output,
/// renders formatted information panels with Mac-style chrome.
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

    /// Render the dashboard panels to the framebuffer.
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);

    /// Update the refresh interval.
    void setInterval(uint16_t interval_ms) { interval_ms_ = interval_ms; }

    /// Update dashboard commands from a server config.
    /// Replaces current commands with the provided ones.
    void updateCommands(const sdcard::DashboardCommand* cmds, int count);

    /// Set the server name displayed in the title bar.
    void setServerName(const char* name);

private:
    /// Maximum number of dashboard commands
    static constexpr int MAX_COMMANDS = 8;
    /// Maximum output buffer per command
    static constexpr int MAX_OUTPUT_LEN = 256;
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
    float cpu_history_[HISTORY_LEN];
    float mem_history_[HISTORY_LEN];
    int history_pos_;
    float cpu_load_[3];         // 1, 5, 15 min load averages
    float mem_percent_;
    float mem_used_gb_;
    float mem_total_gb_;
    float disk_percent_;
    char disk_used_str_[16];
    char disk_total_str_[16];
    char uptime_str_[48];
    char gpu_str_[32];
    char screens_str_[16];
    char server_name_[32];

    /// Parse /littlefs/dashboard.cfg. Returns false if file not found.
    bool loadConfig();

    /// Load built-in default commands.
    void loadDefaults();

    /// Send the next command via SSH.
    void sendNextCommand(ssh::SshClient& ssh);

    /// Parse command outputs into structured fields after sentinel detection.
    void parseOutputs();

    /// Draw sparkline graph.
    void drawSparkline(onebit::IFramebuffer& fb, int x, int y, int w, int h,
                       const float* data, int data_len, int head, float max_val);

    /// Draw a group box with label on top border.
    void drawGroupBox(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                      int x, int y, int w, int h, const char* label);
};

} // namespace app
