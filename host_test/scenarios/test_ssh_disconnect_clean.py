"""Connect + disconnect → heap within 2 KB; no lingering ssh_client task."""
import pathlib
import subprocess

import pytest


@pytest.fixture
def ed25519_keypair(tmp_path):
    priv = tmp_path / "id_ed25519"
    subprocess.check_call(["ssh-keygen", "-q", "-t", "ed25519", "-f", str(priv), "-N", ""])
    return {"priv": priv, "pub": priv.with_suffix(".pub")}


def test_disconnect_leaves_no_leak(fresh_device, loopback_sshd, ed25519_keypair):
    d = fresh_device
    d.ssh_known_hosts_erase()
    pathlib.Path(loopback_sshd["authorized_keys_path"]).write_text(
        ed25519_keypair["pub"].read_text()
    )
    d.send("fs-mkdir /littlefs/keys")  # idempotent (EEXIST is not an error); needed before first write
    d.fs_write("/littlefs/keys/pytest_ed25519", ed25519_keypair["priv"].read_bytes())

    heap_before = d.heap()["free"]

    d.ssh_connect(
        host=loopback_sshd["host"],
        port=loopback_sshd["port"],
        user=loopback_sshd["user"],
        key_path="/littlefs/keys/pytest_ed25519",
    )
    d.wait_for_ssh_state("connected", timeout=15)
    d.ssh_disconnect()
    d.wait_for_ssh_state("disconnected", timeout=5)

    import time
    time.sleep(1.0)  # allow the task to fully exit

    heap_after = d.heap()["free"]
    delta = abs(heap_after - heap_before)
    assert delta < 2048, (
        f"heap delta {delta} B after connect/disconnect (before={heap_before}, after={heap_after})"
    )
