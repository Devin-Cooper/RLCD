#include "dashboard.hpp"
#include "dashboard_feed.hpp"
#include "config_manager.hpp"
#include <esp_log.h>
#include <cstdio>
#include <cstdlib>
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
