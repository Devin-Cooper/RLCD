"""Scenario 14: attempt to inject shell meta-chars via name/pubkey into
the enroll exec command -> rejected at sanitization step, nothing written
to authorized_keys.

Skipped in Phase 15: UI-only flow. The sanitization is exercised from
the Enroll Key screen; no REPL enroll command exists. The underlying
sanitizer DOES have host unit tests (test_ssh_enroll_sanitize*.cpp if
present) covering the character-set check. Captured for the plan record.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only enrollment flow. Phase 15 limit: the "
    "ssh-keys-* REPL only exposes list/pubkey/export/qr-raw/delete/"
    "generate — no enroll command. Captured for the plan record; a "
    "future phase adds a REPL path or UI driver.",
    allow_module_level=True,
)


def test_ssh_keys_enroll_rejects_injection():
    pass  # unreachable — module-level skip
