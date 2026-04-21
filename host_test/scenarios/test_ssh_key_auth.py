"""Key auth via ed25519 key pushed to device with fs_write; authorized_keys on sshd side."""
import pathlib
import subprocess

import pytest


@pytest.fixture
def ed25519_keypair(tmp_path):
    priv = tmp_path / "id_ed25519"
    subprocess.check_call(
        ["ssh-keygen", "-q", "-t", "ed25519", "-f", str(priv), "-N", ""]
    )
    pub = priv.with_suffix(".pub")
    return {"priv": priv, "pub": pub}


def test_ssh_key_auth_roundtrip(fresh_device, loopback_sshd, ed25519_keypair):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Authorize this pubkey on the sshd side.
    authorized = pathlib.Path(loopback_sshd["authorized_keys_path"])
    authorized.write_text(ed25519_keypair["pub"].read_text())

    # Push the private key onto the device via existing chunked upload.
    d.send("fs-mkdir /littlefs/keys")  # idempotent (EEXIST is not an error); needed before first write
    d.fs_write("/littlefs/keys/pytest_ed25519", ed25519_keypair["priv"].read_bytes())

    d.ssh_connect(
        host=loopback_sshd["host"],
        port=loopback_sshd["port"],
        user=loopback_sshd["user"],
        key_path="/littlefs/keys/pytest_ed25519",
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
