"""Connect + disconnect → heap within 2 KB; no lingering ssh_client task.

Uses device-generated keypair (Phase 11) so we can drive key auth via
a KeyStore-indexed ssh_key_id; the old PEM-drop path no longer resolves
through KeyStore::path_for().
"""
import pathlib
import time

import pytest


def test_disconnect_leaves_no_leak(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Clean any leftover keys from prior tests.
    for k in d.ssh_keys_list():
        d.ssh_keys_delete(k["id"])

    ssh_key_id = d.ssh_keys_generate("pytest_disconnect_clean")
    try:
        pubkey_line = d.ssh_keys_pubkey(ssh_key_id)
        pathlib.Path(loopback_sshd["authorized_keys_path"]).write_text(
            pubkey_line + "\n"
        )

        heap_before = d.heap()["free"]

        d.ssh_connect(
            host=loopback_sshd["host"],
            port=loopback_sshd["port"],
            user=loopback_sshd["user"],
            ssh_key_id=ssh_key_id,
        )
        d.wait_for_ssh_state("connected", timeout=15)
        d.ssh_disconnect()
        d.wait_for_ssh_state("disconnected", timeout=5)

        time.sleep(1.0)  # allow the task to fully exit

        heap_after = d.heap()["free"]
        delta = abs(heap_after - heap_before)
        assert delta < 2048, (
            f"heap delta {delta} B after connect/disconnect (before={heap_before}, after={heap_after})"
        )
    finally:
        try:
            d.ssh_keys_delete(ssh_key_id)
        except Exception:
            pass
