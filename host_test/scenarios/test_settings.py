def test_settings_save_persists_in_nvs(fresh_device):
    fresh_device.settings_set("dashboard_interval_ms", 3000)
    assert fresh_device.nvs_get("app_settings", "dash_interval") == ("u16", 3000)

    fresh_device.reboot()
    assert fresh_device.nvs_get("app_settings", "dash_interval") == ("u16", 3000)
