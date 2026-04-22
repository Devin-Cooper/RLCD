"""Scenario 1: empty -> generate -> list shows 1 -> fingerprint matches
ssh-keygen -lf -> delete -> empty.

Exercises the happy-path of the ssh-keys-generate REPL command, which
generates an Ed25519 keypair on-device, stores it in /littlefs/ssh_keys/,
and returns the 32-char hex UUID.
"""
import pathlib
import subprocess

from conftest import ssh_key_fingerprint


def test_ssh_keys_generate(wifi_device, tmp_path):
    d = wifi_device  # wifi_device -> fresh_device + saved SSID; Wi-Fi must be up for keygen gate
    # Start empty
    assert d.ssh_keys_list() == []

    # Generate via REPL (works because Wi-Fi is up)
    uuid = d.ssh_keys_generate("pytest_key_1")
    assert len(uuid) == 32
    assert all(c in "0123456789abcdef" for c in uuid), f"not lowercase hex: {uuid!r}"

    # List shows one
    keys = d.ssh_keys_list()
    assert len(keys) == 1
    assert keys[0]["id"] == uuid
    assert keys[0]["name"] == "pytest_key_1"
    # type glyph is "ed2" for Ed25519 in the REPL output
    assert keys[0]["type"] in ("ed2", "ed25519"), keys[0]

    # Compare SHA256 fingerprint against ssh-keygen -lf
    pubkey_line = d.ssh_keys_pubkey(uuid)
    assert pubkey_line.startswith("ssh-ed25519 "), pubkey_line
    pub_path = tmp_path / "pytest_key_1.pub"
    pub_path.write_text(pubkey_line + "\n")
    fp = ssh_key_fingerprint(pub_path)
    assert fp.startswith("SHA256:"), fp
    # Device fp_head is the first 16 chars of the base64-encoded raw SHA-256
    # digest (without "SHA256:" prefix or padding). ssh-keygen's SHA256:... has
    # the same base64 body — so the device head must be a prefix of the
    # openssh token's body.
    assert keys[0]["fp_head"] == fp[len("SHA256:"):len("SHA256:") + 16], (
        f"device fp_head={keys[0]['fp_head']!r} ssh-keygen fp={fp!r}"
    )

    # Delete
    d.ssh_keys_delete(uuid)
    assert d.ssh_keys_list() == []
