# Migration — factory + ota_0 partition layout

This repo now ships a **factory recovery partition + single OTA slot**
instead of the previous dual-OTA layout. On a first-time checkout of the
`feat/factory-recovery` branch (or `main` once merged) you need to
re-partition your dev board **once**; after that, normal `idf.py -C
esp32_rendering flash` iterating on the main firmware continues to
work without touching recovery or the partition table.

## What changes

| Before | After |
|---|---|
| ota_0 (3 MB at 0x20000) + ota_1 (3 MB at 0x320000) | factory (512 KB at 0x20000) + ota_0 (10 MB at 0xA0000) |
| littlefs (2 MB at 0x620000) | littlefs (4 MB at 0xAA0000) |
| No recovery firmware | Stable 237 KB recovery firmware in factory slot |
| Main slot 45% full (1.35 MB / 3 MB) | Main slot 14% full (1.35 MB / 10 MB) |
| Bootloader has no fallback if main corrupts on flash | Bootloader auto-boots factory when ota_0 fails to mark-valid |

## What survives, what doesn't

**Survives** (nvs/otadata/phy_init offsets are byte-identical):
- `nvs` partition (at 0x9000, unchanged): saved WiFi networks, server table,
  app_settings, legacy ssh_creds.
- `otadata` (at 0x10000): rollback state is preserved.

**Lost** (littlefs is relocated and resized):
- Everything in `/littlefs` — dashboard.cfg, any cached content.

**Coredump partition is also relocated** (0x820000 → 0xEA0000), so any
pending coredump from a prior crash is lost. Not usually a concern.

## Pre-flight: back up littlefs

Before running `flash.sh`, back up any data you care about in `/littlefs`.
The only file we normally care about is `/littlefs/dashboard.cfg`:

```bash
# Starting state: device is running the CURRENT firmware (pre-migration)
# which has test_console enabled.
cd host_test/scenarios
python3 -u -c "
import sys; sys.path.insert(0, '.')
from device import Device
d = Device()
data = d.fs_read('/littlefs/dashboard.cfg')
open('/tmp/dashboard.cfg.bak', 'wb').write(data)
print(f'saved {len(data)} bytes')
d.close()
"
```

## Run the migration

```bash
cd /path/to/RLCD
./flash.sh /dev/cu.usbmodem4101
```

`flash.sh` builds both firmwares, cross-checks the two partition tables
agree on the nvs/otadata/phy_init rows, then writes:

- bootloader (0x0)
- partition table (0x8000)
- factory recovery (0x20000)
- main app (0xA0000)

Per-offset — **no `--fill-flash-size`, no `merge_bin`** — so the NVS at
0x9000 is never touched.

**DO NOT use `idf.py flash` for the migration.** It would try to write
the main binary at 0x20000 (the factory slot) because that's the first
app partition in the new table. Only `./flash.sh` knows to place main
at 0xA0000.

## Expected first boot

1. esptool finishes, device hard-resets.
2. Bootloader sees valid ota_0 and boots it.
3. Main app runs, `boot_validator_task` sees stable DashboardScreen
   within 30 s and calls `esp_ota_mark_app_valid_cancel_rollback()`.
4. WiFi auto-connects (wifi_creds survived).
5. Dashboard comes up empty (dashboard.cfg was on littlefs which got
   wiped). Restore it — see next section.

If main fails to boot (partition-table mismatch, image corrupt,
etc.), the bootloader falls back to factory, you see the
"RECOVERY MODE" banner, and you can either:
- `reboot-ota` from the recovery REPL to try ota_0 again, or
- Reflash ota_0: `idf.py -C esp32_rendering flash` (the dev-mode
  inner loop writes only ota_0 at 0xA0000 plus the partition table,
  which leaves factory untouched).

## Restore dashboard.cfg

```bash
cd host_test/scenarios
python3 -u -c "
import sys; sys.path.insert(0, '.')
from device import Device
d = Device()
with open('/tmp/dashboard.cfg.bak', 'rb') as f:
    data = f.read()
d.fs_write('/littlefs/dashboard.cfg', data)
print(f'wrote {len(data)} bytes')
d.close()
"
```

## Dev inner-loop after migration

Normal iteration on main firmware:

```bash
./dev-flash.sh                    # production config
./dev-flash.sh /dev/cu.usbmodem4101 test   # test-console overlay
```

This builds main and writes ONLY ota_0 at 0xA0000 via esptool directly.
Partition table, bootloader, factory recovery, and NVS are untouched.

**DO NOT use `idf.py -C esp32_rendering flash`** for dev iteration:
ESP-IDF picks the first app partition for its flash offset, which is
`factory` at 0x20000, so `idf.py flash` would **overwrite the recovery
firmware** with main. `dev-flash.sh` bypasses this by driving esptool
at 0xA0000.

Iterating on recovery itself (rare):

```bash
idf.py -C esp32_recovery -p /dev/cu.usbmodem4101 flash
```

Unlike the main case, the recovery project's `idf.py flash` DOES target
0x20000 (the factory slot) because recovery is the first app partition
in the table. Safe for recovery-side changes.

## Troubleshooting

**Device stuck on "RECOVERY MODE"** — main app didn't mark-valid within
30 s. Check for:
- Bad WiFi connect path hanging >30 s before DashboardScreen settles.
  Raise the 30 s window in `esp32_rendering/main/boot_validator.cpp` or
  inspect the boot log for late panics.
- `ota-info` (via test_console on a test build, or recovery's `info`)
  to see otadata state.

**Flash fails with "No serial data received"** — usual esptool download
mode issue. Hold button B (BOOT) at reset; esptool will connect.

**`info` in recovery shows `state=n/a`** — correct when running factory.
Factory has no otadata tracking; state is only relevant for ota_0.

**WiFi creds appear blank** — something erased the nvs partition.
`flash.sh` explicitly avoids this, but if you ran `idf.py erase-flash`
or `esptool erase_flash` at any point, the creds are gone. Re-save:

```bash
python3 -c "
import sys; sys.path.insert(0, 'host_test/scenarios')
from device import Device
d = Device()
d.wifi_save('YourSSID', 'YourPassword')
d.reboot()
d.close()
"
```

**Want to force recovery boot** — hold button A at reset for 5+
seconds. The bootloader erases otadata and boots factory on the next
cycle. Useful for "my ota_0 marked-valid but it's broken anyway."

Equivalent from the host (no physical access to button A):

```bash
esptool --chip esp32s3 -p /dev/cu.usbmodem4101 --after hard_reset \
    erase_region 0x10000 0x2000
```

Erases the otadata partition; next boot falls through to factory.

## Verified on hardware

The full flow was verified on an ESP32-S3-RLCD-4.2 dev board:

- `./flash.sh` writes 512 KB recovery + 1.35 MB main + partition
  table; NVS (WiFi creds) survives.
- Cold boot reaches ota_0 → DashboardScreen → `boot_validator_task`
  transitions otadata state `PENDING` → `VALID` within its 30 s window
  (1 s in test builds).
- `ota-info` from main's test_console reports the current otadata
  state from ota_0.
- Recovery REPL: `ping`, `info`, `reboot-ota`, `erase-nvs --yes`,
  `coredump-dump` all respond correctly.
- `reboot-ota` from recovery returns the device to main.
- `erase_region 0x10000 0x2000` forces factory boot on the next cycle;
  recovery's `info` reports `running_label=factory state=n/a`.
- 14/14 scenario tests pass against the new layout.

Recovery binary ends at 237 KB / 512 KB (45% of the factory slot).
Main ends at 1.35 MB / 10 MB (14% of the ota_0 slot) — 3.3× more
room for feature growth than the prior dual-OTA layout.
