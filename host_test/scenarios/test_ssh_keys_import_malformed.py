"""Scenario 5: drop malformed PEM to SD -> UI import -> rejected with error.

Skipped in Phase 15: the ssh-keys-* REPL surface does NOT expose an import
command. Malformed-input rejection is exercised by the UI Import screen's
error path. Captured for the plan's test-matrix completeness.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only flow. Phase 15 limit: the ssh-keys-* REPL "
    "only exposes list/pubkey/export/qr-raw/delete/generate — no import "
    "command. This test file is captured for the plan record; a future "
    "phase adds a REPL path or integration-test harness.",
    allow_module_level=True,
)


def test_ssh_keys_import_malformed():
    pass  # unreachable — module-level skip
