"""Scenario 4: drop rsa-2048 PEM to SD -> UI import -> pubkey matches fixture.

Skipped in Phase 15: the ssh-keys-* REPL surface does NOT expose an import
command. Importing from SD is driven from the Import Key screen's UI.
Captured for the plan's test-matrix completeness.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only flow. Phase 15 limit: the ssh-keys-* REPL "
    "only exposes list/pubkey/export/qr-raw/delete/generate — no import "
    "command. This test file is captured for the plan record; a future "
    "phase adds a REPL path or integration-test harness.",
    allow_module_level=True,
)


def test_ssh_keys_import_rsa():
    pass  # unreachable — module-level skip
