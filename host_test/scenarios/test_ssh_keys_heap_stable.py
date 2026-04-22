"""Scenario 15: 30x generate+delete; min_free heap delta <= 2 KB vs baseline.

Checks for heap leaks across the generate -> store -> delete cycle. The
generator uses libssh (ed25519 keypair + NVS write + LittleFS pubkey
file); delete tears that down symmetrically. Any net allocation that
escapes the cycle accumulates in min_free.

Uses heap()["min"] (the esp_get_minimum_free_heap_size watermark), which
is monotone-non-increasing across the device's lifetime — a good leak
indicator that ignores fragmentation-only jitter.
"""


def test_ssh_keys_heap_stable(wifi_device):
    d = wifi_device
    # Let any startup residual settle before we snapshot.
    d.ping()

    heap_before = d.heap()
    min_before = heap_before["min"]
    assert min_before > 0, f"heap returned no min_free: {heap_before}"

    # 30 iterations of generate + delete. Keep names unique to avoid any
    # name-collision short-circuit on the firmware side.
    for i in range(30):
        uuid = d.ssh_keys_generate(f"churn_{i}")
        d.ssh_keys_delete(uuid)

    assert d.ssh_keys_list() == [], "churn left keys behind"

    heap_after = d.heap()
    min_after = heap_after["min"]

    # Delta is "how much lower the watermark moved." Can be 0 if the cycle
    # never exceeded the pre-existing watermark. Must be <= 2 KB by spec.
    delta = min_before - min_after
    assert delta <= 2048, (
        f"heap leak? before min={min_before} after min={min_after} "
        f"delta={delta} B (spec: <= 2048 B)"
    )
