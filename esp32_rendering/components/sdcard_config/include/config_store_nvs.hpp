#pragma once
#include "config_manager.hpp"

namespace sdcard {

/// NVS persistence helpers extracted for host-testability (no cJSON dep).
/// Use the `servers` NVS namespace with dense indexing; keys:
///   count (u8)
///   n_<i>, h_<i>, u_<i>, pw_<i>, kp_<i>  (str)
///   p_<i>  (u16, port)
///   ka_<i> (u8, use_key_auth)

bool persistServersToNvs(const ServerRuntime* servers, int count);

/// Populates `out[0..count-1]` from NVS. Returns count loaded (0..max).
int  loadServersFromNvs(ServerRuntime* out, int max);

/// Writes/reads app_settings.active_srv. Read returns 0 if not set.
void persistActiveIndex(int index);
int  loadActiveIndex(int max_valid);

/// Run one-shot legacy migration. `servers[0..*count-1]` must hold
/// SD-loaded identities (if any) on entry. On return:
/// - `*count` may have grown (PathB seeds a single default)
/// - `servers[i].creds.password` may have been populated from legacy
///   ssh_creds NVS namespace
/// - Legacy namespaces erased on successful PathA/BeltAndSuspenders
/// - NVS `servers` namespace persisted if any mutation occurred
/// Returns which branch was taken.
MigrationResult runLegacyMigration(ServerRuntime* servers,
                                    int* count,
                                    int max_servers);

} // namespace sdcard
