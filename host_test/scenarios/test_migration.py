def test_migration_path_b_seeds_default_server(fresh_device):
    # Seed legacy app_settings.ssh_host state. Loopback so post-migration
    # SSH connect fails fast (ECONNREFUSED via lwIP) instead of DNS-hanging
    # or TCP-retry-hanging the ssh_client past the watchdog threshold.
    fresh_device.nvs_set("app_settings", "ssh_host", "str", "127.0.0.1")
    fresh_device.nvs_set("app_settings", "ssh_port", "u16", 2222)
    fresh_device.nvs_set("app_settings", "ssh_user", "str", "dev")

    # Also erase 'servers' NVS so migration has something to do.
    fresh_device.nvs_erase("servers")

    # Generous boot_timeout: this is the second reboot in the test (the
    # fresh_device fixture already did one), and when run at the tail of a
    # full batch the device's NVS/LittleFS state is heavier than in isolation.
    # 60 s covers the worst boot we've observed empirically (~35 s) with
    # plenty of margin without masking a real firmware hang.
    fresh_device.reboot(boot_timeout=60.0)

    assert fresh_device.migration() == "PathB"

    servers = fresh_device.server_list()
    assert len(servers) == 1
    assert servers[0]["name"] == "default"
    assert "127.0.0.1" in servers[0]["endpoint"]
