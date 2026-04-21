"""Bogus known_hosts entry → connect → Error 'HOST KEY CHANGED'; file preserved."""
import pathlib
import subprocess

import pytest


@pytest.fixture
def ed25519_keypair(tmp_path):
    priv = tmp_path / "id_ed25519"
    subprocess.check_call(["ssh-keygen", "-q", "-t", "ed25519", "-f", str(priv), "-N", ""])
    return {"priv": priv, "pub": priv.with_suffix(".pub")}


def test_tofu_change_triggers_error(fresh_device, loopback_sshd, ed25519_keypair):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Pre-seed known_hosts with a known-wrong ed25519 entry for this host:port.
    bogus_pub = (
        f"[{loopback_sshd['host']}]:{loopback_sshd['port']} ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
    )
    d.fs_write("/littlefs/known_hosts", bogus_pub.encode("ascii"))

    # Configure an ed25519 key so the only possible failure path is the TOFU check.
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
    d.wait_for_ssh_state("error", timeout=10)
    assert "HOST KEY CHANGED" in d.ssh_last_error(), d.ssh_last_error()

    # file still has exactly one entry — the bogus one, unchanged.
    entries = d.ssh_known_hosts_list()
    assert len(entries) == 1, entries
