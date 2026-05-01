#pragma once

#include <cstdint>
#include <cstddef>

namespace app {

/// Snapshot of all dashboard data the renderer needs. Owned and refreshed
/// by Dashboard at end-of-cycle (parseOutputs). Read only by the renderer.
/// Single-thread invariant: read+write happens on the foreground task.
struct DashboardSnapshot {
    // Parsed metric values
    float    cpu_load[3]    = {0, 0, 0};
    float    cpu_history[60]= {};
    int      history_pos    = 0;
    float    mem_percent    = 0;
    float    mem_used_gb    = 0;
    float    mem_total_gb   = 0;
    float    mem_history[60]= {};
    float    disk_percent   = 0;
    char     disk_used_str[16]  = {};
    char     disk_total_str[16] = {};
    char     uptime_str[48] = {};
    char     gpu_str[32]    = {};
    char     screens_str[16]= {};

    // Pointers into Dashboard::commands_[i].output (NOT copies — see spec).
    // Length equals command_count; pointers outlive any single read.
    const char* command_outputs[8] = {};
    int         command_count = 0;

    // Chrome metadata
    char     server_name[32] = {};
    int64_t  last_update_ms = 0;
    bool     connected      = false;
    uint16_t interval_ms    = 5000;
};

} // namespace app
