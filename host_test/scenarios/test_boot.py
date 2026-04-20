def test_boots_to_dashboard(fresh_device):
    top = fresh_device.stack_top()
    assert "DashboardScreen" in top, f"unexpected top: {top}"
    assert fresh_device.heap()["free"] > 100_000

def test_reboot_survives_uptime(fresh_device):
    heap_before = fresh_device.heap()["free"]
    up_before = fresh_device.ping()

    fresh_device.reboot()

    heap_after = fresh_device.heap()["free"]
    up_after = fresh_device.ping()
    assert up_after < up_before   # reset
    assert abs(heap_after - heap_before) < 4096
