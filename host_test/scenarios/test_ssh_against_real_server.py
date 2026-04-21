"""E2E against a user-provided SSH server. Env-gated."""
import os

import pytest


@pytest.mark.skipif(
    not (os.environ.get("TEST_SSH_HOST") and os.environ.get("TEST_SSH_USER")),
    reason="set TEST_SSH_HOST/TEST_SSH_USER (and optional TEST_SSH_PASS/TEST_SSH_PORT) to run",
)
def test_real_server_roundtrip(fresh_device):
    d = fresh_device
    d.ssh_known_hosts_erase()

    host = os.environ["TEST_SSH_HOST"]
    user = os.environ["TEST_SSH_USER"]
    password = os.environ.get("TEST_SSH_PASS", "")
    port = int(os.environ.get("TEST_SSH_PORT", "22"))

    d.ssh_connect(host=host, port=port, user=user, password=password)
    try:
        d.wait_for_ssh_state("connected", timeout=20)
    except AssertionError:
        pytest.fail(f"ssh did not reach connected: last_error='{d.ssh_last_error()}'")

    info = d.ssh_info()
    assert info.get("cipher_in")
    assert info.get("hostkey") in (
        "ssh-ed25519", "ecdsa-sha2-nistp256", "rsa-sha2-512", "rsa-sha2-256"
    ), info

    d.ssh_disconnect()
    d.wait_for_ssh_state("disconnected", timeout=5)
