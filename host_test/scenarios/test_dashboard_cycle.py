import time

import pytest


def test_dashboard_auto_advances_on_dwell(one_server_device):
    """Dashboard auto-advances to the next card after dwell elapses."""
    dev = one_server_device
    dev.expect_stack_top("DashboardScreen", timeout=5.0)

    initial = dev.active_card()
    assert initial["count"] >= 2

    # Default dwell is 3000ms; wait 4s and observe advance.
    time.sleep(4.0)
    after = dev.active_card()
    assert after["idx"] != initial["idx"], (
        f"Card did not advance after 4s "
        f"(still on idx={after['idx']} label={after['label']})"
    )


def test_dashboard_arrow_keys_step(one_server_device):
    dev = one_server_device
    dev.expect_stack_top("DashboardScreen", timeout=5.0)

    start = dev.active_card()
    n = start["count"]

    dev.key_arrow("right")
    time.sleep(0.3)
    assert dev.active_card()["idx"] == (start["idx"] + 1) % n

    dev.key_arrow("left")
    time.sleep(0.3)
    assert dev.active_card()["idx"] == start["idx"]


def test_dashboard_digit_jump(one_server_device):
    dev = one_server_device
    dev.expect_stack_top("DashboardScreen", timeout=5.0)

    n = dev.active_card()["count"]
    if n < 3:
        pytest.skip(f"Need >= 3 cards for digit jump test, got {n}")

    dev.key_press("3")  # jump to card index 2
    time.sleep(0.3)
    assert dev.active_card()["idx"] == 2

    dev.key_press("1")
    time.sleep(0.3)
    assert dev.active_card()["idx"] == 0
