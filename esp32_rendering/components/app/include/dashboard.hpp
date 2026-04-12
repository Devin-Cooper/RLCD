#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include "ssh_client.hpp"
#include "settings.hpp"
#include <cstdint>

namespace app {

/// Dashboard mode: runs SSH commands periodically, parses output,
/// renders formatted information panels.
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

private:
    /// Maximum number of dashboard commands
    static constexpr int MAX_COMMANDS = 8;
    /// Maximum output buffer per command
    static constexpr int MAX_OUTPUT_LEN = 256;
    /// Maximum label length
    static constexpr int MAX_LABEL_LEN = 24;

    struct DashboardCommand {
        char label[MAX_LABEL_LEN];
        char command[64];
        char output[MAX_OUTPUT_LEN];
        int output_len;
        bool valid;
    };

    DashboardCommand commands_[MAX_COMMANDS];
    int command_count_;
    int current_command_;       // Index of command currently being collected
    bool collecting_;           // True while receiving output for a command

    uint16_t interval_ms_;
    int64_t last_update_ms_;

    /// Parse /littlefs/dashboard.cfg. Returns false if file not found.
    bool loadConfig();

    /// Load built-in default commands.
    void loadDefaults();

    /// Send the next command via SSH.
    void sendNextCommand(ssh::SshClient& ssh);

    /// Render a single panel section.
    void renderPanel(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                     int x, int y, int w, int h,
                     const char* label, const char* content);
};

} // namespace app
