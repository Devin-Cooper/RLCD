"""Bogus known_hosts entry → connect → Error 'HOST KEY CHANGED'; file preserved.

Uses device-generated keypair (Phase 11) so we can drive key auth via a
KeyStore-indexed ssh_key_id. The load-bearing check is the TOFU
mismatch on the sshd host key, not the client auth path — but we still
need a working key-auth stanza because use_key_auth=true suppresses
password fallback.
"""
import pytest


def test_tofu_change_triggers_error(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Pre-seed known_hosts with a known-wrong ed25519 entry for this host:port.
    bogus_pub = (
        f"[{loopback_sshd['host']}]:{loopback_sshd['port']} ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
    )
    d.fs_write("/littlefs/known_hosts", bogus_pub.encode("ascii"))

    # Clean any leftover keys from prior tests.
    for k in d.ssh_keys_list():
        d.ssh_keys_delete(k["id"])

    ssh_key_id = d.ssh_keys_generate("pytest_tofu_changed")
    try:
        # We don't need to authorize the pubkey on the sshd side here —
        # the ssh session should never reach userauth because the TOFU
        # host-key check fires first and aborts the transport.
        d.ssh_connect(
            host=loopback_sshd["host"],
            port=loopback_sshd["port"],
            user=loopback_sshd["user"],
            ssh_key_id=ssh_key_id,
        )
        d.wait_for_ssh_state("error", timeout=10)
        assert "HOST KEY CHANGED" in d.ssh_last_error(), d.ssh_last_error()

        # file still has exactly one entry — the bogus one, unchanged.
        entries = d.ssh_known_hosts_list()
        assert len(entries) == 1, entries
    finally:
        try:
            d.ssh_keys_delete(ssh_key_id)
        except Exception:
            pass
