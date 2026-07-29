# RLCD de-fork: retiring `components/rendering`

**Status: complete.** Both firmwares build and link. The forked copy of the
1-bit graphics core is gone, and so is the per-pixel adapter that stood between
it and the rest of the application.

**No hardware was attached at any point.** Everything below is host builds,
host tests and source analysis. Nothing here is visual or on-device
confirmation, and the two wire-level changes called out in
[§5](#5-what-did-not-port-cleanly) remain unverified against a real panel.

---

## 1. What was retired, and what replaced it

| Retired | Lines | Replaced by |
|---|---|---|
| `esp32_rendering/components/rendering/` — a copy of onebit's `framebuffer`, `primitives`, `dirty_tracker`, `types` under `namespace rendering` | 795 | Nothing. It was dead: 81 files already used `onebit::`, and only the fork's own `.cpp` files included its headers. |
| `esp32_rendering/components/st7305/` — panel driver taking `rendering::IFramebuffer&` | 491 | `esp32_rendering/components/st7306_panel/` (259 lines), a transport-only subclass of `onebit::St7306Display`. |
| `FramebufferAdapter` in `esp32_rendering/main/main.cpp` | 37 | Deleted. `onebit::IFramebuffer` is passed straight through. |
| `FramebufferAdapter` in `esp32_recovery/main/recovery_screen.cpp` | 32 | Deleted, same. |
| 360 KB of PSRAM lookup tables + `initLUT()` | — | `onebit::St7306Layout`'s closed form, verified against the LUT algorithm for all 120,000 pixels in onebit's `tests/hal/test_st7306_transform.cpp`. |

**1,286 lines deleted from the fork**; 331 inserted, 1,426 deleted across the
whole commit (24 files, net **−1,095**).

Every frame previously went through `FramebufferAdapter`, forwarding
`onebit::IFramebuffer` calls **one pixel at a time** into
`rendering::IFramebuffer`. That indirection is gone in both firmwares.

### The new component

`components/st7306_panel/` supplies only transport. Everything that decides
*which bytes* reach the panel — init sequence, address window, the 2x4-block
scan-out transform, the inversion polarity — lives in onebit and is host-tested
there.

```
board::St7306Panel : onebit::St7306Display
    sendCommand / sendData / sendPixels   -> esp_lcd_panel_io
    delayMs                               -> vTaskDelay
    beginFrame                            -> drain in-flight pixel DMA
    init()                                -> SPI bus, panel io, hard reset,
                                             base init(), clear(WHITE)
```

**The pin map is unchanged and was not touched**: mosi 12, sclk 11, dc 5,
cs 40, rst 41, 10 MHz, `SPI2_HOST`. Reset timing is the shipping driver's
(idle high, low 50 ms, high 200 ms), not the vendor demo's 50/20/50.

### Renamed, deliberately

`st7305` → `st7306_panel`, `st7305::Display` → `board::St7306Panel`,
`st7305::Config` → `board::St7306Pins`. Every call site was being edited
anyway; leaving the old name guarantees someone opens the wrong datasheet. The
die is ST7306-class on three independent grounds (400 gate lines via
`B0h = 0x64`; CASET `0x12..0x2A` and RASET `0x00..0xC7` both outside ST7305's
documented ranges; 300x400 exceeding ST7305's SRAM), and Zephyr's board port
binds `sitronix,st7306`.

---

## 2. `showIfDirty` → `pushDirty`

`st7305::Display::showIfDirty(current, previous)` did three things: `memcmp`
the whole buffer, `show()` if different, `memcpy` current into previous. It
required the caller to own and maintain a second 15 KB framebuffer.

`onebit::DirtyRectTracker` does all three internally and owns its own shadow.
`main.cpp` now reads:

```cpp
onebit::DirtyRectTracker dirty(400, 300);
if (!dirty.isValid()) {
    ESP_LOGW(TAG, "dirty tracker shadow alloc failed; pushing every frame");
}
...
display.pushDirty(fb, dirty.update(fb));
```

`St7306Display::caps()` reports `partialUpdate == false`, so
`DisplayDriver::pushDirty` collapses any non-empty rect list into **one** full
push rather than N, and returns without touching the bus when the list is
empty. That guard in `1bit-display/src/hal/display.cpp` was **left exactly as
it was** — it is load-bearing for this panel.

Two intentional behaviour deltas:

- **Frame 0 now always pushes.** The old code compared against a white-cleared
  `prevFb`, so an all-white first frame silently skipped its push.
  `DirtyRectTracker` starts with `forceFull_ = true`. Strictly better.
- **The `bool` return is gone.** `pushDirty` returns `void`; the sole call site
  ignored the old return value. If it is ever needed:
  `auto l = dirty.update(fb); bool pushed = l.count > 0; display.pushDirty(fb, l);`

Recovery keeps an unconditional `push()` — a 1 Hz heartbeat does not warrant a
second 15 KB shadow.

---

## 3. Cost of a full push: ~12 ms

`St7306Display::writeRegion` **ignores the region and writes the whole frame**.
15,000 bytes at 10 MHz is ~12 ms of bus time, and that is the accepted cost.

A faster windowed path is derivable — the scan-out is column-band-major, so a
full-height X band is one contiguous DMA run — but it has never been run on
hardware and is the largest unvalidated performance claim in the project.
Landing it alongside a de-fork would mean two unproven things at once. **It was
deliberately not implemented.**

---

## 4. Verification

### Both firmwares build

| Target | Result | Binary |
|---|---|---|
| `idf.py -C esp32_recovery build` | exit 0, 0 warnings | 235,280 B (0x39710), 45% of the 512 KB factory slot |
| `idf.py -C esp32_rendering build` | exit 0 | 1,812,848 B (0x1BA970) → `ota_0` |

Clean builds from deleted `build/` directories, ESP-IDF v5.5.2.

**`scripts/verify_builds.sh` passes end to end** from empty build directories —
partition-table drift check, recovery build, recovery < 512 KB assertion, main
production build, *and* the main test-overlay build
(`SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test"`), finishing with
`all builds green; recovery fits.` (exit 0). Note the script does not export
`IDF_PYTHON_ENV_PATH`, so it must be run against build directories it
configured itself or it aborts on a Python-env mismatch; that is a pre-existing
quirk of the script on this machine, not a migration issue.

The `esp32_rendering` build emits 31 warnings. **None come from
`st7306_panel.cpp`**, and none are new. By origin: 20 from vendored
`components/libssh/upstream/` (`-Wimplicit-fallthrough`,
`-Wdeprecated-declarations`, `-Wchar-subscripts`), 2 kconfig-symbol notices from
`sdkconfig.defaults`, 4 from pre-existing app/ssh_keys sources, and 5 from
`main.cpp` — all `-Wmissing-field-initializers` on aggregate-initialised structs
(`esp_vfs_littlefs_conf_t` at `main.cpp:235`, `app::ScreenContext` at
`main.cpp:689`). The `ScreenContext` one is provably pre-existing:
`git show HEAD:.../main.cpp` has the identical `ScreenContext ctx{...}` braced
init omitting the same two `std::function` members, which are assigned
immediately afterwards.

The `esp32_recovery` build emits **zero** warnings.

`check_sizes.py` reports the `esp32_terminal.bin` overflowing the `factory`
partition. That is structural and pre-existing: `factory` (0x80000) holds the
*recovery* image, and the main app is flashed to `ota_0` (0xA00000).
`scripts/verify_builds.sh` correspondingly enforces only the recovery limit.

### Recovery binary size, measured both sides

Built HEAD in a throwaway `git worktree` on the same toolchain:

| | bytes |
|---|---|
| before (HEAD) | 237,904 |
| after | 235,280 |
| **delta** | **−2,624** |

`esp32_recovery/README.md` used to attribute ~60 KB to "the ST7305 driver +
rendering primitives". That was wrong — the linker was already dropping most of
it — and the README has been corrected with the measured figure.

The real saving is RAM, not flash: 240 KB index LUT + 120 KB bit LUT + 15 KB
display buffer in PSRAM, all gone. Internal SRAM rises by ~15 KB
(`DirtyRectTracker::shadow_` now goes through the DMA-internal allocator
alongside `St7306Display::scanout_`); see §5.

### onebit test suite: unchanged

```
cmake -S 1bit-display -B build -G Ninja \
      -DONEBIT_BUILD_TESTS=ON -DONEBIT_WARNINGS_AS_ERRORS=ON
[doctest] test cases:   617 |   617 passed | 0 failed | 0 skipped
[doctest] assertions: 14613 | 14613 passed | 0 failed |
```

Clean build directory, `-Wall -Wextra -Werror`. Identical to the equivalence
gate's baseline. **`git status` in `1bit-display` is empty** — the migration
required zero library changes, which was the point.

### Pre-existing breakage found, not caused, not fixed

`host_test` is red at HEAD, independently of this work: **180/184 pass, 4
fail.**

1. Three targets (`test_trends_card_render`, `test_headline_card_render`,
   `test_dashboard_cycle`) fail to **link** on
   `onebit::AttributeMap::{clear,stampPixel,stampSpan}`.
   `host_test/app/CMakeLists.txt` enumerates onebit sources by hand and lists
   `src/core/framebuffer.cpp` but not `src/core/attribute_map.cpp`, which
   onebit added in its `feat/attribute-colour` merge (188317a).
2. One real assertion failure in the `ble_hid` `translateKeycode` suite.

**Proven pre-existing**, not assumed: I stashed the entire migration, rebuilt
`test_trends_card_render` at HEAD, and reproduced the identical link error.
Independently, `host_test` compiles **no file this migration touched** — a grep
for `screen_context`, `st7306_panel`, `st7305` and `rendering/` across
`host_test/` returns only an unrelated `RLCD_COMPONENTS` path variable and a
stub comment. Left alone deliberately: folding an unrelated CI repair into a
de-fork commit hides both.

---

## 5. What did not port cleanly

1. **A DMA/scan-out race was inherited and is being fixed, not reproduced.**
   `esp_lcd_panel_io_tx_color` *queues* (`spi_device_queue_trans`) and returns;
   the queue is only drained at the top of the next `tx_param`. The old
   `show()` called `convertToDisplayFormat()` — rewriting `displayBuffer_` —
   *before* the `0x2A` that would have drained it, so the previous frame's DMA
   could be reading a buffer being overwritten. A naive port reproduces this
   exactly, since `st7306Convert` likewise precedes `setAddressWindow()`. The
   `beginFrame()` override closes it by draining first. **This is the one
   deliberate behaviour change in the migration.**

2. **SPI parameter transactions changed granularity — unverified on hardware.**
   The old driver sent one `tx_param(-1, &byte, 1)` per parameter byte, so CS
   toggled between every byte. onebit sends the command, then the whole
   parameter block in one transaction. *The byte stream at the panel is
   identical* and the command→param CS edge is unchanged; only the
   inter-parameter CS edges disappear. This is the conventional `esp_lcd` form,
   but it is a wire-level change on a write-only panel with undocumented
   registers (`0x62`, `0xB3`, `0xB4`) and **no hardware to test on**. If the
   panel misbehaves, the mitigation is one loop in
   `St7306Panel::sendData` — one `tx_param` per byte.

3. **Init no longer ends with a clear, so the subclass does it.**
   `onebit::St7306Display::init()` stops at display-on; the old
   `initDisplay()` ended with `clear(false)`. `St7306Panel::init()` calls
   `clear(onebit::WHITE)` immediately after. Miss this and the panel comes up
   showing stale frame memory.

4. **Reset and post-sleep delay are invisible to the library.**
   `St7306Display` has no reset hook — its virtuals are only `sendCommand`,
   `sendData`, `sendPixels`, `delayMs` — so the subclass performs the RST GPIO
   sequence itself before calling `init()`, and overrides `delayMs` with
   `vTaskDelay`. A sleep-out that is not waited on is a classic intermittent
   init bug; both are easy to lose in a migration and both fail as "panel never
   comes up" rather than as a wrong image.

5. **Internal SRAM up ~15 KB, PSRAM down 375 KB.** `onebit::alloc` is globally
   overridden to `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`, and *both*
   `St7306Display::scanout_` and `DirtyRectTracker::shadow_` allocate through
   it. Before: 30 KB internal (`fb` + `prevFb`) + 375 KB PSRAM. After: 45 KB
   internal (`fb` + `scanout_` + `shadow_`) + 0 PSRAM. `scanout_` genuinely
   needs DMA-internal memory; `shadow_` does not — it is only ever `memcmp`'d
   and `memcpy`'d. If the extra 15 KB ever matters, construct the tracker under
   a PSRAM allocator and restore the DMA one immediately after.

6. **`DirtyTracker::computeDirtyRegions()` has no onebit counterpart and cannot
   get one.** It returned `std::vector<DirtyRegion>`, which onebit forbids in
   library code, and emitted full-width row bands where onebit emits
   byte-snapped rects. It had zero callers, so this is a deletion, not a gap —
   but do not go looking for the equivalent.

7. **`ScreenContext::display` is provably never read.** Grepping `ctx.display`,
   `ctx_.display` and `.display.` across both firmwares returns nothing. It was
   retyped to `board::St7306Panel&` rather than deleted, because removing it
   perturbs the aggregate initialiser. Deleting it is a clean follow-up.

---

## 6. Not covered

- **Anything on real hardware.** No panel was attached. Items 1 and 2 above are
  unverified against silicon and will stay that way until someone flashes this.
- **The main app's runtime frame loop.** `pushDirty` semantics come from
  onebit's own host-tested `DisplayDriver`; nothing was executed on the device.
- **The `beginFrame()` DMA drain.** Correct by construction from reading
  `esp_lcd_panel_io_spi.c` and `spi_master.c` — `tx_color` queues and only the
  final chunk raises `on_color_trans_done` — but never observed running.
- **`ScreenContext::display` removal.** Retyped, not deleted; see §5.7.
