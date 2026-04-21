"""90s idle → session still alive (libssh keepalive SSH_MSG_IGNORE)."""
import os
import pathlib
import subprocess
import time

import pytest


@pytest.fixture
def ed25519_keypair(tmp_path):
    priv = tmp_path / "id_ed25519"
    subprocess.check_call(["ssh-keygen", "-q", "-t", "ed25519", "-f", str(priv), "-N", ""])
    return {"priv": priv, "pub": priv.with_suffix(".pub")}


@pytest.mark.skipif(
    os.environ.get("SLOW_TESTS") != "1",
    reason="90-second idle; set SLOW_TESTS=1 to run",
)
def test_keepalive_survives_90s_idle(fresh_device, loopback_sshd, ed25519_keypair):
    d = fresh_device
    d.ssh_known_hosts_erase()
    pathlib.Path(loopback_sshd["authorized_keys_path"]).write_text(
        ed25519_keypair["pub"].read_text()
    )
    d.send("fs-mkdir /littlefs/keys")  # idempotent (EEXIST is not an error); needed before first write
    d.fs_write("/littlefs/keys/pytest_ed25519", ed25519_keypair["priv"].read_bytes())

    d.ssh_connect(
        host=loopback_sshd["host"],
        port=loopback_sshd["port"],
        user=loopback_sshd["user"],
        key_path="/littlefs/keys/pytest_ed25519",
    )
    d.wait_for_ssh_state("connected", timeout=15)

    time.sleep(90)

    assert d.ssh_status().get("state") == "connected", d.ssh_status()
    d.ssh_disconnect()
