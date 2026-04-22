"""Scenario 7: rename preserves uuid + fingerprint; delete blocked if
referenced; allowed after detach.

Skipped in Phase 15: rename is not exposed via the ssh-keys-* REPL (only
list/pubkey/export/qr-raw/delete/generate). Rename happens from the Key
Detail screen's UI. The delete-blocked-if-referenced branch COULD be
partially tested by server_upsert'ing a ServerCreds with an ssh_key_id,
but the REPL ssh-keys-delete command deliberately skips the reference
check (see export_repl.cpp:72 comment), so even that path can't be
exercised through the REPL. Captured for the plan's test-matrix
completeness.
"""
import pytest

pytest.skip(
    "Scenario exercises UI-only rename + REPL deliberately skips the "
    "delete-reference-check (export_repl.cpp: 'tests may need to "
    "force-delete'). Phase 15 limit: rename is not exposed via the "
    "ssh-keys-* REPL. Captured for the plan record; a future phase "
    "adds a rename REPL command and/or a ref-check-enforcing delete.",
    allow_module_level=True,
)


def test_ssh_keys_rename_delete():
    pass  # unreachable — module-level skip
