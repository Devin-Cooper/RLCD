"""Scenario 11: Export -> SD writes /sdcard/export/<name>.pub matching pubkey.

Skipped in Phase 15: SD export is triggered by the Key Detail screen's
"S" key press, which pytest can't drive through the current REPL
surface. The export_sd.cpp logic is reachable only from the UI action
handler. Captured for the plan's test-matrix completeness.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only SD export (Key Detail screen 'S' key). "
    "Phase 15 limit: the ssh-keys-* REPL only exposes list/pubkey/export/"
    "qr-raw/delete/generate — no SD-export command. Captured for the "
    "plan record; a future phase adds a REPL path or UI driver.",
    allow_module_level=True,
)


def test_ssh_keys_sd_export():
    pass  # unreachable — module-level skip
