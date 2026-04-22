"""90s idle → session still alive (libssh keepalive SSH_MSG_IGNORE)."""
import os
import pathlib
import time

import pytest


@pytest.mark.skipif(
    os.environ.get("SLOW_TESTS") != "1",
    reason="90-second idle; set SLOW_TESTS=1 to run",
)
def test_keepalive_survives_90s_idle(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Clean any leftover keys from prior tests.
    for k in d.ssh_keys_list():
        d.ssh_keys_delete(k["id"])

    ssh_key_id = d.ssh_keys_generate("pytest_keepalive")
    try:
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

        time.sleep(90)

        assert d.ssh_status().get("state") == "connected", d.ssh_status()
        d.ssh_disconnect()
    finally:
        try:
            d.ssh_keys_delete(ssh_key_id)
        except Exception:
            pass
