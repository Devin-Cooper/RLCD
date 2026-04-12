#include "dashboard.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <esp_log.h>
#include <cstdio>
#include <cstring>

static const char* TAG = "dashboard";

namespace app {

// ============================================================================
// Sentinel used to delimit command outputs in the SSH stream.
// After each command we send: command ; echo __DASH_END__
// The parser looks for this marker to split outputs.
// ============================================================================
static constexpr const char* SENTINEL = "__DASH_END__";
static constexpr int SENTINEL_LEN = 12;

// ============================================================================
// Construction / init
// ============================================================================

Dashboard::Dashboard()
    : commands_{}
    , command_count_(0)
    , current_command_(0)
    , collecting_(false)
    , need_send_next_(false)
    , interval_ms_(5000)
    , last_update_ms_(0)
{}

void Dashboard::init(const Settings& settings) {
    interval_ms_ = settings.dashboard_interval_ms;

    if (!loadConfig()) {
        loadDefaults();
    }

    ESP_LOGI(TAG, "Dashboard initialized: %d commands, interval %d ms",
             command_count_, interval_ms_);
}

// ============================================================================
// Config loading
// ============================================================================

bool Dashboard::loadConfig() {
    FILE* f = fopen("/littlefs/dashboard.cfg", "r");
    if (!f) {
        ESP_LOGI(TAG, "No /littlefs/dashboard.cfg, using defaults");
        return false;
    }

    command_count_ = 0;
    char line[128];

    while (fgets(line, sizeof(line), f) && command_count_ < MAX_COMMANDS) {
        // Strip newline
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // Skip empty lines and comments
        if (len == 0 || line[0] == '#') continue;

        // Format: "Label|command string"
        char* sep = strchr(line, '|');
        if (!sep) continue;

        *sep = '\0';
        auto& cmd = commands_[command_count_];
        snprintf(cmd.label, MAX_LABEL_LEN, "%s", line);
        snprintf(cmd.command, sizeof(cmd.command), "%s", sep + 1);
        cmd.output[0] = '\0';
        cmd.output_len = 0;
        cmd.valid = true;
        ++command_count_;
    }

    fclose(f);
    ESP_LOGI(TAG, "Loaded %d commands from dashboard.cfg", command_count_);
    return command_count_ > 0;
}

void Dashboard::loadDefaults() {
    struct DefaultCmd { const char* label; const char* cmd; };
    static const DefaultCmd defaults[] = {
        {"Load",    "cat /proc/loadavg | cut -d' ' -f1-3"},
        {"Memory",  "free -h | grep Mem | awk '{print $3\"/\"$2}'"},
        {"Disk",    "df -h / | tail -1 | awk '{print $3\"/\"$2\" (\"$5\")\"}'"},
        {"Uptime",  "uptime -p"},
        {"Temp",    "sensors 2>/dev/null | grep 'Package\\|Tctl' | head -1 | awk '{print $NF}'"},
        {"Screens", "screen -ls 2>/dev/null | grep -c Detached || echo 0"},
    };

    command_count_ = 0;
    for (const auto& d : defaults) {
        if (command_count_ >= MAX_COMMANDS) break;
        auto& cmd = commands_[command_count_];
        snprintf(cmd.label, MAX_LABEL_LEN, "%s", d.label);
        snprintf(cmd.command, sizeof(cmd.command), "%s", d.cmd);
        cmd.output[0] = '\0';
        cmd.output_len = 0;
        cmd.valid = true;
        ++command_count_;
    }
}

// ============================================================================
// Update / data collection
// ============================================================================

void Dashboard::update(ssh::SshClient& ssh, int64_t now_ms) {
    if (ssh.state() != ssh::State::Connected) return;
    if (command_count_ == 0) return;

    // Send next command in the sequence if one is pending
    if (need_send_next_) {
        need_send_next_ = false;
        if (current_command_ < command_count_) {
            sendNextCommand(ssh);
        } else {
            // All commands complete — reset for next cycle
            current_command_ = 0;
            collecting_ = false;
        }
        return;
    }

    // Check if it's time to refresh
    if (collecting_) return;  // Don't start a new cycle while collecting
    if (now_ms - last_update_ms_ < interval_ms_) return;
    last_update_ms_ = now_ms;

    // Start a new collection cycle
    current_command_ = 0;
    collecting_ = true;

    // Clear all outputs
    for (int i = 0; i < command_count_; ++i) {
        commands_[i].output[0] = '\0';
        commands_[i].output_len = 0;
    }

    sendNextCommand(ssh);
}

void Dashboard::sendNextCommand(ssh::SshClient& ssh) {
    if (current_command_ >= command_count_) {
        collecting_ = false;
        return;
    }

    // Send: command ; echo __DASH_END__\n
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%s ; echo %s\n",
                     commands_[current_command_].command, SENTINEL);
    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        ssh.send(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
    }
}

void Dashboard::feedData(const uint8_t* data, size_t len) {
    if (!collecting_ || current_command_ >= command_count_) return;

    auto& cmd = commands_[current_command_];

    for (size_t i = 0; i < len; ++i) {
        char ch = static_cast<char>(data[i]);

        // Append to current command output
        if (cmd.output_len < MAX_OUTPUT_LEN - 1) {
            cmd.output[cmd.output_len++] = ch;
            cmd.output[cmd.output_len] = '\0';
        }

        // Check if sentinel is at the end of the output
        if (cmd.output_len >= SENTINEL_LEN) {
            if (strncmp(cmd.output + cmd.output_len - SENTINEL_LEN,
                        SENTINEL, SENTINEL_LEN) == 0) {
                // Trim sentinel and trailing whitespace
                cmd.output_len -= SENTINEL_LEN;
                while (cmd.output_len > 0 &&
                       (cmd.output[cmd.output_len - 1] == '\n' ||
                        cmd.output[cmd.output_len - 1] == '\r' ||
                        cmd.output[cmd.output_len - 1] == ' ')) {
                    --cmd.output_len;
                }
                cmd.output[cmd.output_len] = '\0';

                // Move to next command — update() will send it on the next tick
                ++current_command_;
                need_send_next_ = true;

                if (current_command_ >= command_count_) {
                    collecting_ = false;
                }
                return;
            }
        }
    }
}

// ============================================================================
// Rendering
// ============================================================================

void Dashboard::renderPanel(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font,
                            int x, int y, int w, int h,
                            const char* label, const char* content) {
    // Panel border
    onebit::drawRect(fb, static_cast<int16_t>(x), static_cast<int16_t>(y),
                     static_cast<int16_t>(w), static_cast<int16_t>(h),
                     onebit::BLACK);

    const int16_t cx = font.glyph_width + 1;
    const int16_t cy = font.glyph_height + 2;

    // Label (top-left, inverted)
    int label_w = static_cast<int>(strlen(label)) * cx + 4;
    onebit::fillRect(fb,
                     static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                     static_cast<int16_t>(label_w), static_cast<int16_t>(cy),
                     onebit::BLACK);
    onebit::drawBitmapText(fb, font,
                           static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 2),
                           label, onebit::WHITE);

    // Content (below label, clipped to panel)
    int16_t content_y = static_cast<int16_t>(y + cy + 3);
    int16_t content_x = static_cast<int16_t>(x + 3);

    // Render content — truncate lines that don't fit
    const char* p = content;
    int max_chars = (w - 6) / cx;
    while (*p && content_y + cy < y + h) {
        // Find end of line
        const char* eol = strchr(p, '\n');
        int line_len = eol ? static_cast<int>(eol - p) : static_cast<int>(strlen(p));
        if (line_len > max_chars) line_len = max_chars;

        // Draw this line (need null-terminated copy)
        char line_buf[64];
        int copy_len = (line_len < 63) ? line_len : 63;
        memcpy(line_buf, p, static_cast<size_t>(copy_len));
        line_buf[copy_len] = '\0';

        onebit::drawBitmapText(fb, font, content_x, content_y, line_buf,
                               onebit::BLACK);

        content_y += cy;
        p = eol ? eol + 1 : p + strlen(p);
    }
}

void Dashboard::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    fb.clear(onebit::WHITE);

    if (command_count_ == 0) {
        onebit::drawBitmapText(fb, font, 10, 10, "No dashboard commands",
                               onebit::BLACK);
        return;
    }

    // Layout: arrange panels in a grid
    // 2 columns for 400px width, rows as needed
    const int16_t margin = 4;
    const int16_t panel_w = (fb.width() - margin * 3) / 2;
    const int16_t panel_h = (fb.height() - margin) / ((command_count_ + 1) / 2) - margin;

    for (int i = 0; i < command_count_; ++i) {
        int col = i % 2;
        int row = i / 2;
        int16_t px = margin + col * (panel_w + margin);
        int16_t py = margin + row * (panel_h + margin);

        renderPanel(fb, font, px, py, panel_w, panel_h,
                    commands_[i].label, commands_[i].output);
    }
}

} // namespace app
