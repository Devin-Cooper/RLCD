# libssh (in-tree ESP-IDF component)

Vendored libssh 0.11.4 sources, ported to ESP-IDF via minimal glue.

## Origin

- Upstream library: libssh 0.11.4 (https://www.libssh.org)
- Port:             ewpa/LibSSH-ESP32 pinned at commit `1dead2f92ef860c53ce03e22c1f3baf17e2cd19c`
- License:          LGPL-2.1 (see `upstream/LICENSE`)

## Layout

- `include/libssh/` — public headers, verbatim from upstream
- `upstream/src/`   — libssh C sources, verbatim from upstream
- `upstream/external/` — bundled Ed25519 and curve25519 reference implementations
  (these are why Ed25519 client auth works here but not in libssh2+mbedTLS)
- `port/`           — ESP-IDF glue: log-callback routing

## Refresh procedure

1. Clone ewpa at the new commit.
2. Replace `upstream/src/*` and `include/libssh/*` with the new sources.
3. Run `idf.py build` and fix any new compile/link failures by adding
   the offending .c file to `LIBSSH_EXCLUDE` in `CMakeLists.txt`.
4. Run the full pytest scenario suite — all SSH scenarios must pass.
5. Update the pin SHA in this README.

## Concurrency note

libssh is not thread-safe across concurrent sessions without
`ssh_threads_set_callbacks`. This firmware uses exactly one SSH
session at a time, so it's safe. The future multi-session spec
will wire the threading callbacks when that changes.
