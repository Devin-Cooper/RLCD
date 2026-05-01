#include "dashboard.hpp"
#include "dashboard_feed.hpp"
#include "config_manager.hpp"
#include <1bit/render/primitives.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/render/pattern.hpp>
#include <1bit/render/pattern_tiles.hpp>
#include <1bit/fonts/term_5x7.hpp>
#include <esp_log.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

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
    , skip_echo_(false)
    , interval_ms_(5000)
    , last_update_ms_(0)
{
    // snapshot_ is zero-initialized by NSDMI defaults in DashboardSnapshot.
}

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
        cmd.overflowed = false;
        cmd.first_byte_ms = 0;
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
        cmd.overflowed = false;
        cmd.first_byte_ms = 0;
        ++command_count_;
    }
}

// ============================================================================
// Update commands from server config
// ============================================================================

void Dashboard::updateCommands(const sdcard::DashboardCommand* cmds, int count) {
    command_count_ = 0;
    for (int i = 0; i < count && i < MAX_COMMANDS; i++) {
        strncpy(commands_[i].label, cmds[i].label, sizeof(commands_[i].label) - 1);
        commands_[i].label[sizeof(commands_[i].label) - 1] = '\0';
        strncpy(commands_[i].command, cmds[i].command, sizeof(commands_[i].command) - 1);
        commands_[i].command[sizeof(commands_[i].command) - 1] = '\0';
        commands_[i].output[0] = '\0';
        commands_[i].output_len = 0;
        commands_[i].valid = true;
        commands_[i].overflowed = false;
        commands_[i].first_byte_ms = 0;
        command_count_++;
    }
    // Reset collection state for new command set
    current_command_ = 0;
    collecting_ = false;
    need_send_next_ = false;
    skip_echo_ = false;
    last_update_ms_ = 0;
    ESP_LOGI(TAG, "Updated dashboard with %d commands", command_count_);
}

void Dashboard::setServerName(const char* name) {
    if (name) {
        strncpy(snapshot_.server_name, name, sizeof(snapshot_.server_name) - 1);
        snapshot_.server_name[sizeof(snapshot_.server_name) - 1] = '\0';
    } else {
        snapshot_.server_name[0] = '\0';
    }
}

// ============================================================================
// Update / data collection
// ============================================================================

void Dashboard::update(ssh::SshClient& ssh, int64_t now_ms) {
    snapshot_.connected = (ssh.state() == ssh::State::Connected);
    snapshot_.last_update_ms = last_update_ms_;
    snapshot_.interval_ms = interval_ms_;

    if (ssh.state() != ssh::State::Connected) return;
    if (command_count_ == 0) return;

    // Spec 04 Bug 2 — overflow-timeout fallback.
    // If the current command's output filled to capacity without finding the
    // sentinel and no new bytes have arrived for OVERFLOW_TIMEOUT_MS, give up
    // on it and advance so the dashboard cycle doesn't hang forever.
    if (collecting_ && current_command_ < command_count_) {
        auto& cmd = commands_[current_command_];
        if (cmd.overflowed && cmd.first_byte_ms != 0 &&
            (now_ms - cmd.first_byte_ms) > OVERFLOW_TIMEOUT_MS) {
            ESP_LOGW(TAG, "cmd[%d] %s: sentinel not found after overflow, advancing",
                     current_command_, cmd.label);
            ++current_command_;
            need_send_next_ = true;
            skip_echo_ = false;
        }
    }

    // Send next command in the sequence if one is pending
    if (need_send_next_) {
        need_send_next_ = false;
        if (current_command_ < command_count_) {
            sendNextCommand(ssh);
        } else {
            // All commands complete — parse outputs and push history
            parseOutputs();
            snapshot_.cpu_history[snapshot_.history_pos % HISTORY_LEN] = snapshot_.cpu_load[0];
            snapshot_.mem_history[snapshot_.history_pos % HISTORY_LEN] = snapshot_.mem_percent;
            snapshot_.history_pos++;

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

    // BLANK SCREEN FIX: Do NOT clear outputs here. Each command's output is
    // cleared in sendNextCommand() just before sending, so a render between
    // clearing and receiving will still show the previous cycle's data for
    // commands not yet re-sent.

    sendNextCommand(ssh);
}

void Dashboard::sendNextCommand(ssh::SshClient& ssh) {
    if (current_command_ >= command_count_) {
        collecting_ = false;
        return;
    }

    // Clear this command's output right before sending (not all at once)
    commands_[current_command_].output[0] = '\0';
    commands_[current_command_].output_len = 0;
    commands_[current_command_].overflowed = false;
    commands_[current_command_].first_byte_ms = 0;
    skip_echo_ = true;

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s ; echo %s\n",
                     commands_[current_command_].command, SENTINEL);
    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        ssh.send(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
        ESP_LOGD(TAG, "Sent cmd[%d]: %s", current_command_,
                 commands_[current_command_].label);
    }
}

void Dashboard::feedData(const uint8_t* data, size_t len) {
    if (!collecting_ || current_command_ >= command_count_) return;

    auto& cmd = commands_[current_command_];

    // Spec 04 Bug 2: timestamp first real byte so update() can time-out an
    // overflowed command. Any byte arriving past skip_echo counts.
    if (cmd.first_byte_ms == 0 && len > 0) {
        // Only stamp if we'll actually process bytes (skip_echo eats them
        // until the first newline; we still consider the first byte that
        // makes it past as the "first byte" for timeout purposes).
        int64_t now_ms = esp_timer_get_time() / 1000;
        cmd.first_byte_ms = now_ms;
    }

    FeedBuffer buf{cmd.output, cmd.output_len, MAX_OUTPUT_LEN, skip_echo_};
    FeedStatus st = feedChunk(buf, data, len);

    // Detect overflow: buffer filled up but sentinel not yet found.
    if (st == FeedStatus::Continue &&
        buf.output_len >= MAX_OUTPUT_LEN - 1 && !cmd.overflowed) {
        cmd.overflowed = true;
        ESP_LOGW(TAG, "cmd[%d] %s: output exceeded %d bytes, awaiting timeout",
                 current_command_, cmd.label, MAX_OUTPUT_LEN - 1);
    }

    cmd.output_len = buf.output_len;
    skip_echo_     = buf.skip_echo;

    if (st == FeedStatus::Complete) {
        ESP_LOGD(TAG, "Got output[%d] %s: '%.*s'",
                 current_command_, cmd.label,
                 cmd.output_len > 40 ? 40 : cmd.output_len, cmd.output);

        ++current_command_;
        need_send_next_ = true;

        if (current_command_ >= command_count_) {
            parseOutputs();
            snapshot_.cpu_history[snapshot_.history_pos % HISTORY_LEN] = snapshot_.cpu_load[0];
            snapshot_.mem_history[snapshot_.history_pos % HISTORY_LEN] = snapshot_.mem_percent;
            snapshot_.history_pos++;

            collecting_ = false;
            ESP_LOGI(TAG, "Dashboard cycle complete");
        }
    }
}

// ============================================================================
// Parse command outputs into structured fields
// ============================================================================

void Dashboard::parseOutputs() {
    for (int i = 0; i < command_count_; ++i) {
        const char* label = commands_[i].label;
        const char* out = commands_[i].output;

        if (strcasecmp(label, "Load") == 0 || strcasecmp(label, "CPU") == 0) {
            // "0.10 0.16 0.11" or "0.10 0.16 0.11 1/1030 35150"
            sscanf(out, "%f %f %f", &snapshot_.cpu_load[0], &snapshot_.cpu_load[1], &snapshot_.cpu_load[2]);
        }
        else if (strcasecmp(label, "Memory") == 0 || strcasecmp(label, "Mem") == 0) {
            // "3.8Gi/30Gi" or "3.8G/30G" or "380Mi/30Gi"
            float used = 0, total = 0;
            char used_unit[8] = {}, total_unit[8] = {};
            // Try parsing "X.XGi/Y.YGi" or "X.XG/Y.YG" or "XMi/YGi"
            if (sscanf(out, "%f%7[^/]/%f%7s", &used, used_unit, &total, total_unit) >= 3) {
                // Normalize to GB
                if (used_unit[0] == 'M' || used_unit[0] == 'm') used /= 1024.0f;
                if (used_unit[0] == 'T' || used_unit[0] == 't') used *= 1024.0f;
                if (total_unit[0] == 'M' || total_unit[0] == 'm') total /= 1024.0f;
                if (total_unit[0] == 'T' || total_unit[0] == 't') total *= 1024.0f;
                snapshot_.mem_used_gb = used;
                snapshot_.mem_total_gb = total;
                snapshot_.mem_percent = (total > 0) ? (used / total) * 100.0f : 0;
            }
        }
        else if (strcasecmp(label, "Disk") == 0) {
            // "956G/3.6T (28%)" or "200G/500G (40%)"
            char used_s[16] = {}, total_s[16] = {};
            int pct = 0;
            if (sscanf(out, "%15[^/]/%15s (%d%%)", used_s, total_s, &pct) >= 2) {
                strncpy(snapshot_.disk_used_str, used_s, sizeof(snapshot_.disk_used_str) - 1);
                snapshot_.disk_used_str[sizeof(snapshot_.disk_used_str) - 1] = '\0';
                // Strip trailing paren from total if present
                char* paren = strchr(total_s, '(');
                if (paren) *paren = '\0';
                strncpy(snapshot_.disk_total_str, total_s, sizeof(snapshot_.disk_total_str) - 1);
                snapshot_.disk_total_str[sizeof(snapshot_.disk_total_str) - 1] = '\0';
                snapshot_.disk_percent = static_cast<float>(pct);
            }
            // Fallback: try just percentage
            if (snapshot_.disk_percent == 0) {
                const char* pct_ptr = strchr(out, '%');
                if (pct_ptr) {
                    // Scan backwards to find start of number
                    const char* num_start = pct_ptr - 1;
                    while (num_start > out && (*(num_start-1) >= '0' && *(num_start-1) <= '9'))
                        --num_start;
                    snapshot_.disk_percent = static_cast<float>(atoi(num_start));
                }
            }
        }
        else if (strcasecmp(label, "Uptime") == 0) {
            strncpy(snapshot_.uptime_str, out, sizeof(snapshot_.uptime_str) - 1);
            snapshot_.uptime_str[sizeof(snapshot_.uptime_str) - 1] = '\0';
            // Strip leading "up " if present
            if (strncmp(snapshot_.uptime_str, "up ", 3) == 0) {
                memmove(snapshot_.uptime_str, snapshot_.uptime_str + 3, strlen(snapshot_.uptime_str + 3) + 1);
            }
        }
        else if (strcasecmp(label, "Temp") == 0 || strcasecmp(label, "GPU") == 0) {
            strncpy(snapshot_.gpu_str, out, sizeof(snapshot_.gpu_str) - 1);
            snapshot_.gpu_str[sizeof(snapshot_.gpu_str) - 1] = '\0';
        }
        else if (strcasecmp(label, "Screens") == 0) {
            strncpy(snapshot_.screens_str, out, sizeof(snapshot_.screens_str) - 1);
            snapshot_.screens_str[sizeof(snapshot_.screens_str) - 1] = '\0';
        }
    }

    // Refresh the snapshot's command_outputs view (pointer copy, not byte copy)
    snapshot_.command_count = command_count_;
    for (int i = 0; i < command_count_ && i < 8; ++i) {
        snapshot_.command_outputs[i] = commands_[i].output;
    }
    for (int i = command_count_; i < 8; ++i) {
        snapshot_.command_outputs[i] = nullptr;
    }
}

// ============================================================================
// Drawing helpers
// ============================================================================

void Dashboard::drawSparkline(onebit::IFramebuffer& fb, int x, int y, int w, int h,
                              const float* data, int data_len, int head, float max_val) {
    if (max_val <= 0) max_val = 1;
    if (w <= 0 || h <= 0 || data_len <= 0) return;

    int prev_vy = -1;

    // Scale horizontally: map w pixels to data_len samples
    for (int px = 0; px < w; px++) {
        // Map pixel position to data index
        int sample_idx = (px * data_len) / w;
        int idx = (head - data_len + sample_idx + data_len) % data_len;
        float val = data[idx];
        if (val > max_val) val = max_val;
        int vy = y + h - 1 - static_cast<int>((val / max_val) * (h - 1));

        // Fill below the line with dithered pattern
        if (vy < y + h) {
            onebit::fillPatternRect(fb, x + px, vy, 1, y + h - vy,
                                    onebit::bayer(128, 4));
        }

        // Draw the line point
        fb.setPixel(x + px, vy, onebit::BLACK);

        // Connect to previous point
        if (prev_vy >= 0) {
            onebit::drawLine(fb, x + px - 1, prev_vy, x + px, vy, onebit::BLACK);
        }
        prev_vy = vy;
    }
}

void Dashboard::drawGroupBox(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                             int x, int y, int w, int h, const char* label) {
    onebit::drawRect(fb, x, y, w, h, onebit::BLACK);
    // Knockout area for label on top border
    int lw = onebit::getBitmapTextWidth(font, label, 1);
    onebit::fillRect(fb, x + 6, y - 1, lw + 4, 3, onebit::WHITE);
    onebit::drawBitmapText(fb, font, x + 8, y - 3, label, onebit::BLACK, 1);
}

// ============================================================================
// Rendering — Mac-style dashboard
// ============================================================================

static int s_render_count = 0;

void Dashboard::render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) {
    s_render_count++;
    if (s_render_count % 50 == 0) {
        ESP_LOGD(TAG, "render #%d: cmd_count=%d collecting=%d cur_cmd=%d hist_pos=%d cpu=%.2f mem=%.0f%%",
                 s_render_count, command_count_, collecting_, current_command_,
                 snapshot_.history_pos, snapshot_.cpu_load[0], snapshot_.mem_percent);
    }

    fb.clear(onebit::WHITE);

    if (command_count_ == 0) {
        onebit::drawBitmapText(fb, font, 10, 10, "No dashboard commands",
                               onebit::BLACK);
        return;
    }

    // Use 5x7 font for dense info display
    const auto& f = onebit::fonts::TERM_5X7;
    const int16_t cx = f.glyph_width + 1;  // char cell width

    // ================================================================
    // Window border (full screen 400x300)
    // ================================================================
    onebit::drawRect(fb, 0, 0, 400, 300, onebit::BLACK);

    // ================================================================
    // Title Bar (y=1 to y=15, 15px tall) — active window stripes
    // ================================================================
    for (int row = 1; row < 16; row += 2)
        onebit::drawLine(fb, 1, row, 398, row, onebit::BLACK);

    // Close box (left)
    onebit::fillRect(fb, 3, 3, 11, 11, onebit::WHITE);
    onebit::drawRect(fb, 3, 3, 11, 11, onebit::BLACK);
    onebit::drawRect(fb, 5, 5, 7, 7, onebit::BLACK);

    // Title text with knockout background — show server name
    {
        char title[64];
        if (snapshot_.server_name[0]) {
            snprintf(title, sizeof(title), "Homelab (%s)", snapshot_.server_name);
        } else {
            snprintf(title, sizeof(title), "Dashboard");
        }
        int tw = onebit::getBitmapTextWidth(font, title, 1);
        int tx = (400 - tw) / 2;
        onebit::fillRect(fb, tx - 4, 1, tw + 8, 14, onebit::WHITE);
        onebit::drawBitmapText(fb, font, tx, 4, title, onebit::BLACK, 1);
    }

    // ================================================================
    // Metric Bars (y=18 downward) — CPU, Memory, Disk
    // ================================================================
    int content_x = 3;
    int content_w = 394;
    int bar_x = 52;
    int bar_w = 210;

    // --- Row 1: CPU load bar ---
    {
        int row_y = 19;
        onebit::drawBitmapText(fb, f, content_x + 2, row_y + 6, "CPU",
                               onebit::BLACK, 1);

        // Compute CPU percentage from load average (normalized to ~100%)
        // Use load[0] / some reasonable baseline; cap at 100
        float cpu_pct = snapshot_.cpu_load[0] * 100.0f;
        if (cpu_pct > 100.0f) cpu_pct = 100.0f;
        if (cpu_pct < 0.0f) cpu_pct = 0.0f;

        onebit::drawRect(fb, bar_x, row_y + 4, bar_w, 11, onebit::BLACK);
        int fill_w = static_cast<int>((bar_w - 2) * cpu_pct / 100.0f);
        if (fill_w > 0)
            onebit::fillRect(fb, bar_x + 1, row_y + 5, fill_w, 9, onebit::BLACK);
        int remain = bar_w - 2 - fill_w;
        if (remain > 0)
            onebit::fillPatternRect(fb, bar_x + 1 + fill_w, row_y + 5, remain, 9,
                                    onebit::bayer(64, 4));

        char pct_str[8];
        snprintf(pct_str, sizeof(pct_str), "%3.0f%%", cpu_pct);
        onebit::drawBitmapText(fb, f, bar_x + bar_w + 4, row_y + 6, pct_str,
                               onebit::BLACK, 1);

        char detail[48];
        snprintf(detail, sizeof(detail), "Load: %.1f %.1f %.1f",
                 snapshot_.cpu_load[0], snapshot_.cpu_load[1], snapshot_.cpu_load[2]);
        onebit::drawBitmapText(fb, f, bar_x + bar_w + 32, row_y + 6, detail,
                               onebit::BLACK, 1);
    }

    // --- Row 2: Memory bar ---
    {
        int row_y = 39;
        onebit::drawBitmapText(fb, f, content_x + 2, row_y + 6, "Memory",
                               onebit::BLACK, 1);

        float pct = snapshot_.mem_percent;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;

        onebit::drawRect(fb, bar_x, row_y + 4, bar_w, 11, onebit::BLACK);
        int fill_w = static_cast<int>((bar_w - 2) * pct / 100.0f);
        if (fill_w > 0)
            onebit::fillRect(fb, bar_x + 1, row_y + 5, fill_w, 9, onebit::BLACK);
        int remain = bar_w - 2 - fill_w;
        if (remain > 0)
            onebit::fillPatternRect(fb, bar_x + 1 + fill_w, row_y + 5, remain, 9,
                                    onebit::bayer(64, 4));

        char pct_str[8];
        snprintf(pct_str, sizeof(pct_str), "%3.0f%%", pct);
        onebit::drawBitmapText(fb, f, bar_x + bar_w + 4, row_y + 6, pct_str,
                               onebit::BLACK, 1);

        char detail[32];
        snprintf(detail, sizeof(detail), "%.1f/%.0fG", snapshot_.mem_used_gb, snapshot_.mem_total_gb);
        onebit::drawBitmapText(fb, f, bar_x + bar_w + 32, row_y + 6, detail,
                               onebit::BLACK, 1);
    }

    // --- Row 3: Disk bar ---
    {
        int row_y = 59;
        onebit::drawBitmapText(fb, f, content_x + 2, row_y + 6, "Disk /",
                               onebit::BLACK, 1);

        float pct = snapshot_.disk_percent;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;

        onebit::drawRect(fb, bar_x, row_y + 4, bar_w, 11, onebit::BLACK);
        int fill_w = static_cast<int>((bar_w - 2) * pct / 100.0f);
        if (fill_w > 0)
            onebit::fillRect(fb, bar_x + 1, row_y + 5, fill_w, 9, onebit::BLACK);
        int remain = bar_w - 2 - fill_w;
        if (remain > 0)
            onebit::fillPatternRect(fb, bar_x + 1 + fill_w, row_y + 5, remain, 9,
                                    onebit::bayer(64, 4));

        char pct_str[8];
        snprintf(pct_str, sizeof(pct_str), "%3.0f%%", pct);
        onebit::drawBitmapText(fb, f, bar_x + bar_w + 4, row_y + 6, pct_str,
                               onebit::BLACK, 1);

        char detail[32];
        if (snapshot_.disk_used_str[0] && snapshot_.disk_total_str[0]) {
            snprintf(detail, sizeof(detail), "%s/%s", snapshot_.disk_used_str, snapshot_.disk_total_str);
        } else {
            detail[0] = '\0';
        }
        onebit::drawBitmapText(fb, f, bar_x + bar_w + 32, row_y + 6, detail,
                               onebit::BLACK, 1);
    }

    // ================================================================
    // Divider line
    // ================================================================
    int div_y = 80;
    onebit::drawLine(fb, 1, div_y, 398, div_y, onebit::BLACK);

    // ================================================================
    // CPU History sparkline (full width)
    // ================================================================
    int cpu_box_y = div_y + 3;
    int cpu_box_h = 90;
    drawGroupBox(fb, f, 3, cpu_box_y, content_w, cpu_box_h, "CPU History");
    {
        int gx = 5, gy = cpu_box_y + 6;
        int gw = content_w - 4;
        int gh = cpu_box_h - 18;

        // Find max for scaling — at least 1.0
        float max_val = 1.0f;
        for (int i = 0; i < HISTORY_LEN; i++) {
            if (snapshot_.cpu_history[i] > max_val) max_val = snapshot_.cpu_history[i];
        }
        max_val *= 1.2f;

        drawSparkline(fb, gx, gy, gw, gh,
                      snapshot_.cpu_history, HISTORY_LEN, snapshot_.history_pos, max_val);

        onebit::drawBitmapText(fb, f, 5, cpu_box_y + cpu_box_h - 10, "60s",
                               onebit::BLACK, 1);
        char cpu_now[24];
        snprintf(cpu_now, sizeof(cpu_now), "now %.2f", snapshot_.cpu_load[0]);
        int nw = onebit::getBitmapTextWidth(f, cpu_now, 1);
        onebit::drawBitmapText(fb, f, content_w - nw, cpu_box_y + cpu_box_h - 10,
                               cpu_now, onebit::BLACK, 1);
    }

    // ================================================================
    // Memory History sparkline (full width)
    // ================================================================
    int mem_box_y = cpu_box_y + cpu_box_h + 3;
    int mem_box_h = 70;
    drawGroupBox(fb, f, 3, mem_box_y, content_w, mem_box_h, "Memory History");
    {
        int gx = 5, gy = mem_box_y + 6;
        int gw = content_w - 4;
        int gh = mem_box_h - 18;

        drawSparkline(fb, gx, gy, gw, gh,
                      snapshot_.mem_history, HISTORY_LEN, snapshot_.history_pos, 100.0f);

        onebit::drawBitmapText(fb, f, 5, mem_box_y + mem_box_h - 10, "60s",
                               onebit::BLACK, 1);
        char mem_now[24];
        snprintf(mem_now, sizeof(mem_now), "now %.0f%%", snapshot_.mem_percent);
        int nw = onebit::getBitmapTextWidth(f, mem_now, 1);
        onebit::drawBitmapText(fb, f, content_w - nw, mem_box_y + mem_box_h - 10,
                               mem_now, onebit::BLACK, 1);
    }

    // ================================================================
    // Bottom panels: System (left) + Info (right)
    // ================================================================
    int info_y = mem_box_y + mem_box_h + 3;
    int info_h = 298 - info_y;
    if (info_h < 20) info_h = 20;
    int half_w = content_w / 2 - 2;

    // --- System Info (left half) ---
    drawGroupBox(fb, f, 3, info_y, half_w, info_h, "System");
    {
        int ix = 7, iy = info_y + 6;
        int line_h = f.glyph_height + 3;

        char buf[64];
        if (snapshot_.uptime_str[0]) {
            snprintf(buf, sizeof(buf), "Up: %s", snapshot_.uptime_str);
            // Truncate long uptime text to fit
            int max_chars = (half_w - 10) / cx;
            if (static_cast<int>(strlen(buf)) > max_chars)
                buf[max_chars] = '\0';
            onebit::drawBitmapText(fb, f, ix, iy, buf, onebit::BLACK, 1);
        }

        snprintf(buf, sizeof(buf), "Screens: %s", snapshot_.screens_str[0] ? snapshot_.screens_str : "N/A");
        onebit::drawBitmapText(fb, f, ix, iy + line_h, buf, onebit::BLACK, 1);

        snprintf(buf, sizeof(buf), "Load: %.1f  %.1f  %.1f",
                 snapshot_.cpu_load[0], snapshot_.cpu_load[1], snapshot_.cpu_load[2]);
        onebit::drawBitmapText(fb, f, ix, iy + line_h * 2, buf, onebit::BLACK, 1);
    }

    // --- Network / GPU Info (right half) ---
    int net_x = 3 + half_w + 4;
    int net_w = content_w - half_w - 4;
    drawGroupBox(fb, f, net_x, info_y, net_w, info_h, "Info");
    {
        int ix = net_x + 4, iy = info_y + 6;
        int line_h = f.glyph_height + 3;

        char buf[48];
        snprintf(buf, sizeof(buf), "Temp: %s", snapshot_.gpu_str[0] ? snapshot_.gpu_str : "N/A");
        onebit::drawBitmapText(fb, f, ix, iy, buf, onebit::BLACK, 1);

        // Show raw disk output if we have it
        if (snapshot_.disk_used_str[0]) {
            snprintf(buf, sizeof(buf), "Disk: %s/%s", snapshot_.disk_used_str, snapshot_.disk_total_str);
            onebit::drawBitmapText(fb, f, ix, iy + line_h, buf, onebit::BLACK, 1);
        }

        snprintf(buf, sizeof(buf), "Mem: %.1f/%.0fG", snapshot_.mem_used_gb, snapshot_.mem_total_gb);
        onebit::drawBitmapText(fb, f, ix, iy + line_h * 2, buf, onebit::BLACK, 1);
    }

}

// ============================================================================
// Command accessor
// ============================================================================

Dashboard::CommandView Dashboard::commandAt(int i) const {
    CommandView v{};
    if (i < 0 || i >= command_count_) {
        v.label = "";
        v.command = "";
        v.output = "";
        v.output_len = 0;
        v.valid = false;
        v.overflowed = false;
        return v;
    }
    const auto& c = commands_[i];
    v.label = c.label;
    v.command = c.command;
    v.output = c.output;
    v.output_len = c.output_len;
    v.valid = c.valid;
    v.overflowed = c.overflowed;
    return v;
}

} // namespace app
