"""Empty known_hosts → connect → known_hosts now has one ed25519 entry.

Uses device-generated keypair (Phase 11) so we can drive key auth via a
KeyStore-indexed ssh_key_id; the old /littlefs/keys/<name> PEM-drop path
no longer resolves through KeyStore::path_for().
"""
import pathlib

import pytest


def test_tofu_first_connect_persists_key(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()
    assert d.ssh_known_hosts_list() == []

    # Clean any leftover keys from prior tests.
    for k in d.ssh_keys_list():
        d.ssh_keys_delete(k["id"])

    ssh_key_id = d.ssh_keys_generate("pytest_tofu_first")
    try:
        # Authorize this pubkey on the sshd side so the key auth actually
        # succeeds — this is a green-path TOFU-acceptance test.
        pubkey_line = d.ssh_keys_pubkey(ssh_key_id)
        pathlib.Path(loopback_sshd["authorized_keys_path"]).write_text(
            pubkey_line + "\n"
        )

        d.ssh_connect(
            host=loopback_sshd["host"],
            port=loopback_sshd["port"],
            user=loopback_sshd["user"],
            ssh_key_id=ssh_key_id,
        )
        d.wait_for_ssh_state("connected", timeout=15)

        entries = d.ssh_known_hosts_list()
        assert len(entries) == 1
        assert entries[0]["key_type"] == "ssh-ed25519"

        d.ssh_disconnect()
    finally:
        try:
            d.ssh_keys_delete(ssh_key_id)
        except Exception:
            pass
