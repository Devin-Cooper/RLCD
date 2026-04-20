"""Protocol client for the ESP32-S3 test_console REPL."""

from __future__ import annotations

import collections
import dataclasses
import os
import re
import threading
import time
from typing import Any, Optional

import serial


@dataclasses.dataclass
class Response:
    ok: bool
    code: Optional[int] = None    # ERR code, or None on OK
    value: Optional[str] = None   # payload after OK/ERR
    data_lines: list[str] = dataclasses.field(default_factory=list)


_MARKER_RE = re.compile(r'^>>>\s+(OK|ERR|DATA)(?:\s+(.*))?$')


class Device:
    DEFAULT_PORT = "/dev/cu.usbmodem4101"

    def __init__(
        self,
        port: Optional[str] = None,
        baud: Optional[int] = None,
    ) -> None:
        port = port or os.environ.get("TEST_CONSOLE_PORT", self.DEFAULT_PORT)
        baud = baud or int(os.environ.get("TEST_CONSOLE_BAUD", "460800"))

        # Open with auto-reset SUPPRESSED (spec §Device __init__ constraints).
        self.ser = serial.Serial(
            port, baud,
            timeout=0.1,
            dsrdtr=False, rtscts=False,
            exclusive=True,
        )
        self.ser.setDTR(False)   # GPIO0 — stay high, do not enter bootloader
        self.ser.setRTS(False)   # EN — stay high, do not reset the chip

        self._log_buf: collections.deque[str] = collections.deque(maxlen=500)
        self._marker_lines: collections.deque[str] = collections.deque()
        self._marker_cond = threading.Condition()
        self._reader_thread = threading.Thread(
            target=self._reader_loop, daemon=True
        )
        self._stop = threading.Event()
        self._reader_thread.start()

    def close(self) -> None:
        self._stop.set()
        self.ser.close()

    # -----------------------------------------------------------------
    # Reader thread: splits stream into lines, routes '>>>' into marker
    # queue, everything else into log buffer (for crash post-mortem).
    # -----------------------------------------------------------------
    def _reader_loop(self) -> None:
        buf = b""
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(256)
            except serial.SerialException:
                return
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.rstrip(b"\r").decode("utf-8", errors="replace")
                if text.startswith(">>>"):
                    with self._marker_cond:
                        self._marker_lines.append(text)
                        self._marker_cond.notify_all()
                else:
                    self._log_buf.append(text)

    # -----------------------------------------------------------------
    # Protocol: send command, block for OK/ERR terminal, collect DATA.
    # -----------------------------------------------------------------
    def send(self, cmd: str, *, timeout: float = 5.0) -> Response:
        self.ser.write(cmd.encode() + b"\r\n")
        self.ser.flush()
        deadline = time.time() + timeout
        data_lines: list[str] = []
        while True:
            with self._marker_cond:
                while not self._marker_lines:
                    remain = deadline - time.time()
                    if remain <= 0:
                        raise TimeoutError(f"command '{cmd}' timed out")
                    self._marker_cond.wait(timeout=min(remain, 1.0))
                line = self._marker_lines.popleft()
            m = _MARKER_RE.match(line)
            if not m:
                continue
            kind, value = m.group(1), m.group(2)
            if kind == "DATA":
                data_lines.append(value or "")
            elif kind == "OK":
                return Response(ok=True, value=value, data_lines=data_lines)
            elif kind == "ERR":
                parts = (value or "").split(" ", 1)
                code = int(parts[0]) if parts and parts[0] else -1
                msg = parts[1] if len(parts) > 1 else ""
                return Response(
                    ok=False, code=code, value=msg, data_lines=data_lines
                )

    def wait_for_boot(self, timeout: float = 15.0) -> None:
        """Block until 'Entering main loop' appears on the log stream."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            if any("Entering main loop" in l for l in self._log_buf):
                # Drain any pre-boot marker noise
                with self._marker_cond:
                    self._marker_lines.clear()
                return
            time.sleep(0.05)
        raise TimeoutError("device did not report boot within timeout")

    @property
    def log_tail(self) -> list[str]:
        return list(self._log_buf)
