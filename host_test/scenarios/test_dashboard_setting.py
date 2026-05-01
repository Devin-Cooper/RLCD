import time

import pytest


def test_card_dwell_persists_via_nvs(one_server_device):
    """Setting card_dwell to 1500ms via NVS persists across reboot and is
    actually honoured by the Dashboard auto-advance loop."""
    dev = one_server_device
    dev.expect_stack_top("DashboardScreen", timeout=5.0)

    # Write 1500ms via the existing NVS REPL command. NVS namespace is
    # "app_settings", key "card_dwell" (u16).
    dev.nvs_set("app_settings", "card_dwell", "u16", 1500)
    dev.reboot()
    dev.expect_stack_top("DashboardScreen", timeout=10.0)

    initial = dev.active_card()
    n = initial["count"]
    if n < 2:
        pytest.skip(f"Need >= 2 cards, got {n}")

    # 1500ms dwell -> wait 2s and the card must have advanced
    time.sleep(2.0)
    after = dev.active_card()
    assert after["idx"] != initial["idx"]


def test_card_dwell_clamps_low_at_load(one_server_device):
    """dashboard_card_dwell_ms = 0 in NVS clamps to 1000 when settings load."""
    dev = one_server_device
    dev.nvs_set("app_settings", "card_dwell", "u16", 0)
    dev.reboot()
    dev.expect_stack_top("DashboardScreen", timeout=10.0)

    initial = dev.active_card()
    n = initial["count"]
    if n < 2:
        pytest.skip(f"Need >= 2 cards, got {n}")

    # Effective dwell should be 1000ms (settings clamps load-side per Task 4).
    # Wait 1.5s and confirm the card advanced.
    time.sleep(1.5)
    after = dev.active_card()
    assert after["idx"] != initial["idx"]
