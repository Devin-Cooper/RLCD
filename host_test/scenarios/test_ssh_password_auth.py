"""Password auth roundtrip against loopback sshd."""
import os

import pytest


@pytest.mark.skipif(
    "TEST_SSH_LOCAL_PASSWORD" not in os.environ,
    reason="set TEST_SSH_LOCAL_PASSWORD if local account accepts password auth",
)
def test_ssh_password_auth_roundtrip(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()
    d.ssh_connect(
        host=loopback_sshd["host"],
        port=loopback_sshd["port"],
        user=loopback_sshd["user"],
        password=os.environ["TEST_SSH_LOCAL_PASSWORD"],
    )
    d.wait_for_ssh_state("connected", timeout=15)
    info = d.ssh_info()
    assert info.get("cipher_in")
    assert "ed25519" in info.get("hostkey", ""), info
    d.ssh_disconnect()
    d.wait_for_ssh_state("disconnected", timeout=5)
