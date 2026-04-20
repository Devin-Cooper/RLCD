import pytest
import time

def test_pairing_screen_timeout_pops(fresh_device):
    fresh_device.button("a", "long")
    fresh_device.expect_stack_top("PairingScreen")

    # Inject BLE state disconnected to simulate timeout-without-connect.
    fresh_device.system_ble_state("disconnected")
    fresh_device.expect_stack_top("DashboardScreen", timeout=3.0)


@pytest.mark.slow
def test_pairing_screen_real_timeout_pops(fresh_device):
    """Exercises the real 30s BLE pairing timeout path (not synthetic).

    Runs the actual startPairing(30) code in pairing_screen.cpp; takes
    ~31-35 seconds. Skipped by default; run with `pytest -m slow`.
    """
    fresh_device.button("a", "long")
    fresh_device.expect_stack_top("PairingScreen")
    time.sleep(32)
    # Timeout should have fired; PairingScreen popped.
    top = fresh_device.stack_top()
    assert "PairingScreen" not in top, f"still on pairing after 32s: {top}"
