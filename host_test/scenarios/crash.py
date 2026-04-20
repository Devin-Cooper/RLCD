from __future__ import annotations

import os
import subprocess
import tempfile
import time

from .device import Device


PANIC_MARKERS = [
    "Guru Meditation Error:",
    "abort() was called at PC",
    "Backtrace:",
]


class DeviceCrashError(AssertionError):
    pass


def check_for_crash(device: Device) -> tuple[bool, str]:
    """Scan the device's log ring buffer for a panic marker.

    Returns (crashed, matched_marker).
    """
    tail = device.log_tail
    for line in tail:
        for m in PANIC_MARKERS:
            if m in line:
                return (True, line)
    return (False, "")


def rts_reset(device: Device) -> None:
    """Pulse RTS only (EN) to hard-reset without entering download mode.

    See spec §Heartbeat fallback — send_break() does NOT reset the S3.
    """
    device.ser.setDTR(False)   # GPIO0 stays high
    device.ser.setRTS(True)    # EN low → reset
    time.sleep(0.1)
    device.ser.setRTS(False)   # EN high → running
    time.sleep(0.05)


def extract_coredump(device: Device) -> str:
    """Extract and decode the coredump. Returns human-readable backtrace.

    Requires RLCD_ELF_PATH env var or build/rlcd.elf relative to repo.
    """
    check = device.send("coredump-check", timeout=2.0)
    if not check.ok:
        return "(no coredump partition available)"
    if not (check.value or "").startswith("yes"):
        return "(no coredump present)"

    r = device.send("coredump-read", timeout=120.0)
    if not r.ok:
        return f"(coredump-read failed: {r.value})"

    b64_payload = "".join(r.data_lines)
    with tempfile.NamedTemporaryFile(
        suffix=".b64", delete=False, mode="w"
    ) as tf:
        tf.write(b64_payload)
        tempfile_path = tf.name

    elf_path = os.environ.get(
        "RLCD_ELF_PATH",
        os.path.join(
            os.path.dirname(__file__), "..", "..",
            "esp32_rendering", "build", "rlcd.elf",
        ),
    )
    try:
        result = subprocess.run(
            ["esp-coredump", "info_corefile", "-t", "b64",
             "-c", tempfile_path, elf_path],
            capture_output=True, text=True, timeout=60,
        )
        return result.stdout + "\n---\n" + result.stderr
    except FileNotFoundError:
        return f"(esp-coredump not installed; b64 at {tempfile_path})"
    except subprocess.TimeoutExpired:
        return f"(esp-coredump timed out; b64 at {tempfile_path})"
    finally:
        device.send("coredump-erase", timeout=2.0)
