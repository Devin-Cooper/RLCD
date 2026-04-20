def test_migration_path_b_seeds_default_server(fresh_device):
    # Seed legacy app_settings.ssh_host state.
    fresh_device.nvs_set("app_settings", "ssh_host", "str", "example.com")
    fresh_device.nvs_set("app_settings", "ssh_port", "u16", 2222)
    fresh_device.nvs_set("app_settings", "ssh_user", "str", "dev")

    # Also erase 'servers' NVS so migration has something to do.
    fresh_device.nvs_erase("servers")

    fresh_device.reboot()

    assert fresh_device.migration() == "PathB"

    servers = fresh_device.server_list()
    assert len(servers) == 1
    assert servers[0]["name"] == "default"
    assert "example.com" in servers[0]["endpoint"]
