import time
import pytest


def test_dashboard_disconnected_renders_dim_headline(one_server_device):
    """When SSH is disconnected, the dashboard still renders cards but
    flips the pattern band to BlueNoise-32 (sparse). We can't easily
    assert on pixel patterns over the wire, so this test verifies:
    1. The dashboard remains on screen (no crash, no automatic pop).
    2. The active card index continues to advance (cycling does NOT
       depend on the SSH connection).
    3. The dashboard-active-card REPL still responds.
    """
    dev = one_server_device
    dev.expect_stack_top("DashboardScreen", timeout=5.0)

    # one_server_device uses 127.0.0.1:22 -> SSH is in a perpetual
    # connecting/refused state. The dashboard's connected flag should be false.
    initial = dev.active_card()
    n = initial["count"]
    if n < 2:
        pytest.skip(f"Need >= 2 cards, got {n}")

    time.sleep(4.0)  # default dwell + headroom
    after = dev.active_card()
    assert after["idx"] != initial["idx"]

    # Confirm the dashboard didn't pop out from under us
    assert "DashboardScreen" in dev.stack_top()
