#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize ESP-IDF glue for libssh: installs the log callback and
/// calls `ssh_init()`. Safe to call multiple times.
void libssh_port_init(void);

#ifdef __cplusplus
}
#endif
