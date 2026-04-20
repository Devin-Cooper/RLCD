import time

def test_menu_cycle_all_items(fresh_device):
    fresh_device.button("a", "short")
    fresh_device.expect_stack_top("MenuScreen")
    # 6 items — arrow down 6x should wrap to the first (Dashboard).
    for _ in range(6):
        fresh_device.key_arrow("down")
    fresh_device.key_esc()
    fresh_device.expect_stack_top("DashboardScreen")

def test_font_cycle_through_three_sizes(fresh_device):
    fresh_device.button("a", "short")
    fresh_device.expect_stack_top("MenuScreen")
    fresh_device.key_arrow("down")   # Dashboard(0) -> Terminal(1)
    fresh_device.key_enter()
    fresh_device.expect_stack_top("TerminalScreen")

    # fresh_device just wiped app_settings — font_size key may not exist yet.
    # Firmware loadSettings() falls back to defaultSettings().font_size (=1)
    # on a missing key without writing back (NVS wear avoidance), so we
    # anchor initial to 1 when nvs-get returns ERR 3 (key not found).
    try:
        initial = fresh_device.nvs_get("app_settings", "font_size")[1]
    except AssertionError:
        initial = 1
    for _ in range(3):
        fresh_device.button("b", "short")
        time.sleep(0.2)
    after = fresh_device.nvs_get("app_settings", "font_size")[1]
    assert after == initial

def test_menu_open_close_100x_no_leak(fresh_device):
    heap_before = fresh_device.heap()["free"]
    for _ in range(100):
        fresh_device.button("a", "short")
        fresh_device.key_esc()
    heap_after = fresh_device.heap()["free"]
    assert heap_before - heap_after < 2048, \
        f"leak: before={heap_before} after={heap_after}"
