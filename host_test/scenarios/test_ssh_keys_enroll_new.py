"""Scenario 12: empty authorized_keys -> enrollment -> verify -> ServerCreds
flipped to use_key=true -> subsequent connect uses key auth.

Skipped in Phase 15: the enrollment flow is driven by the Enroll Key
screen's UI — the user enters their server password, the device exec's
an append-to-authorized_keys command, verifies the push by re-auth'ing
with the key, then flips the ServerCreds record. No REPL command
exposes this pipeline. Captured for the plan record.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only enrollment flow. Phase 15 limit: the "
    "ssh-keys-* REPL only exposes list/pubkey/export/qr-raw/delete/"
    "generate — no enroll command. Captured for the plan record; a "
    "future phase adds a REPL path or UI driver.",
    allow_module_level=True,
)


def test_ssh_keys_enroll_new():
    pass  # unreachable — module-level skip
