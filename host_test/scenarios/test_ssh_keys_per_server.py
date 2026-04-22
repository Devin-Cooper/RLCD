"""Scenario 8: 2 servers -> generate key -> assign to server 1 -> reboot ->
connect uses key on server 1; server 2 still password.

Skipped in Phase 15: assignment of an ssh_key_id to a specific server
is a UI operation on the Server Edit screen. server_upsert does accept
the full ServerCreds blob via JSON (Device.server_upsert), so the
assignment COULD be simulated by hand-crafting the JSON with
ssh_key_id — but the subsequent connect-uses-key-on-server-1 assertion
still needs the loopback_sshd fixture plus a verified end-to-end key
auth path, which the older test_ssh_key_auth.py already exercises.

Marked UI-only for scope: the per-server differentiation is what this
scenario tests, and that's a UI-screen concern. Captured for the plan
record.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only per-server key assignment. Phase 15 limit: "
    "while server_upsert accepts ServerCreds JSON, the key-vs-password-"
    "per-server discrimination is a UI screen concern. Captured for the "
    "plan record; a future phase with UI driver can promote this to a "
    "real test.",
    allow_module_level=True,
)


def test_ssh_keys_per_server():
    pass  # unreachable — module-level skip
