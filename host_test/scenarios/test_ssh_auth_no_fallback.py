"""use_key_auth=true + missing key file → Error; no silent password fallback."""
import pytest


def test_missing_key_does_not_fall_back_to_password(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()

    d.ssh_connect(
        host=loopback_sshd["host"],
        port=loopback_sshd["port"],
        user=loopback_sshd["user"],
        key_path="/littlefs/keys/does_not_exist",
    )
    d.wait_for_ssh_state("error", timeout=10)
    assert "Private key unreadable" in d.ssh_last_error(), d.ssh_last_error()

    # Never reached Connected.
    info = d.ssh_info()
    assert info.get("fp", "") == "", info
