def _open_server_list(device):
    device.button("a", "short")
    device.expect_stack_top("MenuScreen")
    # Menu order: Dashboard(0), Terminal(1), Servers(2)
    for _ in range(2):
        device.key_arrow("down")
    device.key_enter()
    device.expect_stack_top("ServerListScreen")


def test_server_list_shift_d_deletes_with_confirm(fresh_device):
    # Loopback gives fast ECONNREFUSED — any other unreachable target
    # either DNS-hangs or TCP-retry-hangs the ssh_client past the watchdog.
    fresh_device.server_upsert({"name":"a", "host":"127.0.0.1", "port":22, "username":"u", "password":""})
    fresh_device.server_upsert({"name":"b", "host":"127.0.0.1", "port":23, "username":"u", "password":""})
    assert len(fresh_device.server_list()) == 2

    _open_server_list(fresh_device)
    fresh_device.key_press("D")   # capital D — confirm modal appears

    import time; time.sleep(0.2)
    fresh_device.key_enter()      # Yes on confirm
    time.sleep(0.3)

    after = fresh_device.server_list()
    assert len(after) == 1


def test_server_edit_save_active_no_reconnect(one_server_device):
    active_before = one_server_device.ssh_status().get("state")

    _open_server_list(one_server_device)
    one_server_device.key_enter()   # open ServerEditScreen on active server
    one_server_device.expect_stack_top("ServerEditScreen")

    # Tab past name to Host (field 1). Overwrite "newhost".
    one_server_device.key_tab()
    for _ in range(10):
        one_server_device.key_backspace()
    one_server_device.key_press("newhost")

    # Tab through Port/User/Pw/Save (3 tabs -> Save row = focus 5).
    for _ in range(4):
        one_server_device.key_tab()
    one_server_device.key_enter()   # Save

    import time; time.sleep(0.3)
    # SSH should NOT have auto-reconnected.
    active_after = one_server_device.ssh_status().get("state")
    assert active_after == active_before
