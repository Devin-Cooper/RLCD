#pragma once

#include <cstdint>
#include <cstddef>

namespace app {

/// Snapshot of all dashboard data the renderer needs. Owned by Dashboard,
/// read only by the renderer.
///
/// Refresh cadence:
///   - Parsed metrics + command_outputs[]: written at end-of-cycle by
///     parseOutputs() (i.e. once per dashboard refresh interval).
///   - Chrome metadata (connected, last_update_ms, interval_ms): refreshed
///     at the top of every Dashboard::update() tick so disconnect events
///     show up immediately rather than waiting for the next parse.
///
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
    //
    // Lifetime invariant: stable while Dashboard::commands_ is in-place
    // storage and the Dashboard instance outlives the read. Do not move
    // or reallocate commands_ without invalidating these pointers.
    const char* command_outputs[8] = {};
    int         command_count = 0;

    // Chrome metadata
    char     server_name[32] = {};
    int64_t  last_update_ms = 0;
    bool     connected      = false;
    uint16_t interval_ms    = 5000;
};

} // namespace app
