"""Scenario 6: generate 3 -> reboot -> list still 3 with unchanged fingerprints.

Exercises KeyStore NVS persistence + /littlefs/ssh_keys/<id>.pub survival
across esp_restart(). fresh_device preserves wifi_creds, so after the
reboot the device can still generate should we need to (we don't — we
only read back).

The firmware ssh_creds namespace is wiped by _erase_app_state() in
conftest.py, so NVS keys for ssh_keys must live in a DIFFERENT namespace
(Phase 3: "ssh_keys" / "ssh_keys_v1" per the design). Otherwise the
fresh_device fixture would delete them before each test.
"""


def test_ssh_keys_persist(wifi_device):
    d = wifi_device
    # Pre-condition: empty
    assert d.ssh_keys_list() == []

    # Generate 3 keys
    uuids = []
    pubkeys_before = []
    for i in range(3):
        u = d.ssh_keys_generate(f"persist_key_{i}")
        uuids.append(u)
        pubkeys_before.append(d.ssh_keys_pubkey(u))

    # Sanity: 3 keys with matching uuids
    before_list = d.ssh_keys_list()
    assert len(before_list) == 3
    assert sorted(k["id"] for k in before_list) == sorted(uuids)

    # Reboot via the canonical helper — Device.reboot() handles USB-JTAG
    # re-enumeration and calls wait_for_boot() automatically.
    d.reboot()

    # List should still have 3 keys, same UUIDs.
    keys_after = d.ssh_keys_list()
    assert len(keys_after) == 3
    assert sorted(k["id"] for k in keys_after) == sorted(uuids)

    # Verify pubkey bytes survive exactly — fingerprint is derived from
    # pubkey bytes, so matching pubkey ⇒ matching fingerprint.
    for u, pk in zip(uuids, pubkeys_before):
        assert d.ssh_keys_pubkey(u) == pk, f"pubkey changed across reboot for {u}"

    # Cleanup
    for u in uuids:
        d.ssh_keys_delete(u)
    assert d.ssh_keys_list() == []
