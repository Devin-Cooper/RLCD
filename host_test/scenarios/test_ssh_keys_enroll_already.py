"""Scenario 13: authorized_keys already contains pubkey -> enrollment's
try-auth-first short-circuits -> ServerCreds flipped without re-push.

Skipped in Phase 15: UI-only flow (see test_ssh_keys_enroll_new.py for
detail). Captured for the plan record.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only enrollment flow. Phase 15 limit: the "
    "ssh-keys-* REPL only exposes list/pubkey/export/qr-raw/delete/"
    "generate — no enroll command. Captured for the plan record; a "
    "future phase adds a REPL path or UI driver.",
    allow_module_level=True,
)


def test_ssh_keys_enroll_already():
    pass  # unreachable — module-level skip
