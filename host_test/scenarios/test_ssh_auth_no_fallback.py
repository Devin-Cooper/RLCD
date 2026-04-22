"""use_key_auth=true + missing key id → Error; no silent password fallback.

Phase 11 retired the free-form private-key path in favor of a 32-char
hex ssh_key_id that must be indexed in the in-memory KeyStore. A
fabricated id that isn't in the store surfaces as "Private key id not
in store" rather than the old "Private key unreadable" (which would
still fire today if the store had an index entry but the backing file
was gone on disk).
"""
import pytest


def test_missing_key_does_not_fall_back_to_password(fresh_device, loopback_sshd):
    d = fresh_device
    d.ssh_known_hosts_erase()

    # Clean any leftover keys from prior tests so the fabricated id
    # below reliably resolves to "not in store."
    for k in d.ssh_keys_list():
        d.ssh_keys_delete(k["id"])

    # A well-formed-but-nonexistent 32-char hex id. The firmware rejects
    # the auth with "Private key id not in store: <id>" and must NOT
    # silently fall back to password auth.
    bogus_id = "deadbeefcafef00d0123456789abcdef"

    d.ssh_connect(
        host=loopback_sshd["host"],
        port=loopback_sshd["port"],
        user=loopback_sshd["user"],
        ssh_key_id=bogus_id,
    )
    d.wait_for_ssh_state("error", timeout=10)
    last = d.ssh_last_error()
    assert "Private key id not in store" in last or "Private key unreadable" in last, last

    # Never reached Connected.
    info = d.ssh_info()
    assert info.get("fp", "") == "", info
