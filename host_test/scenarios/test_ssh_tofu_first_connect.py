"""Empty known_hosts → connect → known_hosts now has one ed25519 entry."""
import pathlib
import subprocess

import pytest


@pytest.fixture
def ed25519_keypair(tmp_path):
    priv = tmp_path / "id_ed25519"
    subprocess.check_call(["ssh-keygen", "-q", "-t", "ed25519", "-f", str(priv), "-N", ""])
    return {"priv": priv, "pub": priv.with_suffix(".pub")}


def test_tofu_first_connect_persists_key(fresh_device, loopback_sshd, ed25519_keypair):
    d = fresh_device
    d.ssh_known_hosts_erase()
    assert d.ssh_known_hosts_list() == []

    # Use key auth so we have a reliable green path.
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

    entries = d.ssh_known_hosts_list()
    assert len(entries) == 1
    assert entries[0]["key_type"] == "ssh-ed25519"

    d.ssh_disconnect()
