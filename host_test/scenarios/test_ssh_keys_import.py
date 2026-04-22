"""Scenario 3: drop ed25519 PEM to SD -> UI import -> pubkey matches fixture.

Skipped in Phase 15: the ssh-keys-* REPL surface (list/pubkey/export/qr-raw/
delete/generate) does NOT expose an import command. Importing from SD is
driven from the Import Key screen's UI, which pytest can't exercise without
a UI driver. Captured as a scenario file for the plan's test-matrix
completeness; a future phase can add ssh-keys-import <path> and promote
this to a real test.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only flow. Phase 15 limit: the ssh-keys-* REPL "
    "only exposes list/pubkey/export/qr-raw/delete/generate — no import "
    "command. This test file is captured for the plan record; a future "
    "phase adds a REPL path or integration-test harness.",
    allow_module_level=True,
)


def test_ssh_keys_import():
    pass  # unreachable — module-level skip
