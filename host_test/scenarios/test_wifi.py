def _open_wifi_screen(device):
    device.button("a", "short")
    device.expect_stack_top("MenuScreen")
    # Menu order: Dashboard, Terminal, Servers, Settings, WiFi, About
    for _ in range(4):
        device.key_arrow("down")
    device.key_enter()
    device.expect_stack_top("WifiScreen")


def test_wifi_screen_open_close(fresh_device):
    _open_wifi_screen(fresh_device)
    fresh_device.key_esc()
    fresh_device.expect_stack_top("DashboardScreen")


def test_password_screen_type_submit_wrong_rejects(wifi_device):
    _open_wifi_screen(wifi_device)

    # Inject canned scan with one secured AP.
    wifi_device.system_wifi_scan_result("SecuredNet", -60, "wpa2")
    wifi_device.system_wifi_scan_inject()
    import time; time.sleep(0.2)

    # Enter on secured AP → PasswordScreen pushed.
    wifi_device.key_enter()
    wifi_device.expect_stack_top("PasswordScreen")

    wifi_device.key_press("wrongpw")
    wifi_device.key_enter()
    # Connect fires → now simulate terminal-disconnect (reason 202 = auth fail).
    wifi_device.system_wifi_state("disconnected", reason=202)

    # PasswordScreen should remain (user can retry); overlay has Error modal.
    import time; time.sleep(0.5)
    top = wifi_device.stack_top()
    assert "PasswordScreen" in top
