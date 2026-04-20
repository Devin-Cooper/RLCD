"""End-to-end smoke: device responds to ping, uptime grows."""

def test_ping_responds(device):
    u1 = device.ping()
    assert u1 > 0

def test_uptime_increases(device):
    t1 = device.ping()
    import time
    time.sleep(0.5)
    t2 = device.ping()
    assert t2 > t1
    assert t2 - t1 > 400_000   # at least 400ms in us
