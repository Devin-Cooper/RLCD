"""Key auth roundtrip: device generates keypair, host installs pubkey, connect.

Phase 11 retired the "push PEM into /littlefs/keys/<name>" path: the
on-device ssh client now takes a 32-char hex ssh_key_id that must be
indexed in the in-memory KeyStore. Dropping a PEM at
/littlefs/ssh_keys/<hex> without a matching index entry causes the
resolver to fail with "Private key id not in store." So the harness
asks the device to generate its own keypair via the REPL, pulls the
public half back out, and installs it in the sshd fixture's
authorized_keys. This also exercises the generate path end-to-end.
"""
import pathlib

import pytest


def test_ssh_key_auth_roundtrip(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Clean any leftover keys from prior tests before starting.
    for k in d.ssh_keys_list():
        d.ssh_keys_delete(k["id"])

    # Generate a fresh keypair on-device. The generate REPL command gates
    # on live Wi-Fi; fresh_device's wifi_creds are preserved from the
    # session-scoped seed, so the device should be associated already.
    ssh_key_id = d.ssh_keys_generate("pytest_auth_key")
    assert len(ssh_key_id) == 32 and all(
        c in "0123456789abcdef" for c in ssh_key_id
    ), f"unexpected key id: {ssh_key_id!r}"

    try:
        # Pull the pubkey from device and authorize it on the sshd side.
        pubkey_line = d.ssh_keys_pubkey(ssh_key_id)
        authorized = pathlib.Path(loopback_sshd["authorized_keys_path"])
        authorized.write_text(pubkey_line + "\n")

        d.ssh_connect(
            host=loopback_sshd["host"],
            port=loopback_sshd["port"],
            user=loopback_sshd["user"],
            ssh_key_id=ssh_key_id,
        )
        try:
            d.wait_for_ssh_state("connected", timeout=15)
        except AssertionError:
            pytest.fail(f"ssh did not reach connected: last_error='{d.ssh_last_error()}'")

        info = d.ssh_info()
        assert info.get("cipher_in"), f"cipher_in not set: {info}"
        assert "ed25519" in info.get("hostkey", ""), info

        d.ssh_disconnect()
        d.wait_for_ssh_state("disconnected", timeout=5)
    finally:
        # Cleanup the generated key so neighboring tests start clean.
        try:
            d.ssh_keys_delete(ssh_key_id)
        except Exception:
            pass
