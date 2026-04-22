"""Scenario 16: mid-enroll Wi-Fi disconnect -> ServerCreds unchanged, error
surfaced on the Enroll screen.

Gated with SLOW_TESTS=1 per the spec. Also skipped as UI-only: the
enrollment flow itself is driven from the Enroll Key screen, so pytest
can't interleave a Wi-Fi disconnect mid-flow without a UI driver.
Captured for the plan record.
"""
import os
import pytest

pytestmark = pytest.mark.skipif(
    not os.environ.get("SLOW_TESTS"),
    reason="slow test; set SLOW_TESTS=1 to run",
)


def test_ssh_keys_disconnect_during_enroll(fresh_device):
    pytest.skip(
        "Enrollment flow is UI-only; pytest can't drive the Enroll Key "
        "screen through the current REPL surface, let alone interleave "
        "a Wi-Fi disconnect mid-flow. Captured for the plan record; a "
        "future phase adds a UI driver to promote this to a real test."
    )
