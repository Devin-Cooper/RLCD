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

        # ESP32-S3 built-in USB-JTAG CDC: the old UART-style "drop DTR/RTS to
        # suppress auto-reset" dance actively triggers the reset it was meant
        # to prevent — the USB-JTAG peripheral interprets DTR/RTS edges as
        # reset signals. Leave them at pyserial's defaults after open.
        self.ser = serial.Serial(
            port, baud,
            timeout=0.1,
            dsrdtr=False, rtscts=False,
            exclusive=True,
        )

        self._log_buf: collections.deque[str] = collections.deque(maxlen=500)
        self._marker_lines: collections.deque[str] = collections.deque()
        self._marker_cond = threading.Condition()
        self._crashed = False                     # set True by reader on panic marker
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
                for m in ("Guru Meditation Error:",
                          "abort() was called at PC",
                          "Backtrace:"):
                    if m in text:
                        with self._marker_cond:
                            self._crashed = True
                            self._marker_cond.notify_all()
                        break
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
                while not self._marker_lines and not self._crashed:
                    remain = deadline - time.time()
                    if remain <= 0:
                        raise TimeoutError(f"command '{cmd}' timed out")
                    self._marker_cond.wait(timeout=min(remain, 1.0))
                if self._crashed:
                    from .crash import DeviceCrashError
                    raise DeviceCrashError(f"device panicked during '{cmd}'")
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
                    self._crashed = False          # clear after successful reboot
                return
            time.sleep(0.05)
        raise TimeoutError("device did not report boot within timeout")

    @property
    def log_tail(self) -> list[str]:
        return list(self._log_buf)

    # -----------------------------------------------------------------
    # Input injection
    # -----------------------------------------------------------------
    def button(self, which: str, kind: str = "short") -> None:
        r = self.send(f"btn {which} {kind}")
        assert r.ok, r

    def key_press(self, text: str) -> None:
        r = self.send(f"key-press {text}")
        assert r.ok, r

    def key_enter(self) -> None:
        assert self.send("key-enter").ok

    def key_esc(self) -> None:
        assert self.send("key-esc").ok

    def key_tab(self) -> None:
        assert self.send("key-tab").ok

    def key_backspace(self) -> None:
        assert self.send("key-backspace").ok

    def key_arrow(self, direction: str) -> None:
        assert self.send(f"key-arrow {direction}").ok

    def key_fn(self, n: int) -> None:
        assert self.send(f"key-fn {n}").ok

    def key_ctrl(self, letter: str) -> None:
        assert self.send(f"key-ctrl {letter}").ok

    def key_raw(self, hex_bytes: str) -> None:
        assert self.send(f"key-raw {hex_bytes}").ok

    def system_wifi_state(self, state: str, reason: int = 0) -> None:
        assert self.send(f"system-wifi-state {state} {reason}").ok

    def system_ble_state(self, state: str) -> None:
        assert self.send(f"system-ble-state {state}").ok

    def system_wifi_scan_result(self, ssid: str, rssi: int, auth: str = "wpa2") -> None:
        assert self.send(f"system-wifi-scan-result {ssid} {rssi} {auth}").ok

    def system_wifi_scan_inject(self) -> None:
        assert self.send("system-wifi-scan-inject").ok

    # -----------------------------------------------------------------
    # Introspection
    # -----------------------------------------------------------------
    def stack(self) -> list[tuple[int, str, bool]]:
        """Returns list of (depth, typename, is_opaque)."""
        r = self.send("stack")
        assert r.ok, r
        out = []
        for line in r.data_lines:
            parts = line.split(" ", 2)
            out.append((int(parts[0]), parts[1], parts[2] == "opaque"))
        return out

    def stack_top(self) -> str:
        s = self.stack()
        return s[-1][1] if s else ""

    def expect_stack_top(self, name_substr: str, timeout: float = 2.0) -> None:
        deadline = time.time() + timeout
        last = None
        while time.time() < deadline:
            last = self.stack_top()
            if name_substr in last:
                return
            time.sleep(0.05)
        raise AssertionError(
            f"expected stack top containing '{name_substr}', got '{last}'"
        )

    def heap(self) -> dict[str, int]:
        r = self.send("heap")
        assert r.ok, r
        out = {}
        for kv in (r.value or "").split():
            k, _, v = kv.partition("=")
            out[k] = int(v)
        return out

    def wifi_status(self) -> dict[str, Any]:
        r = self.send("wifi-status")
        assert r.ok, r
        out: dict[str, Any] = {}
        for kv in (r.value or "").split():
            k, _, v = kv.partition("=")
            out[k] = int(v) if k == "rssi" else v
        return out

    def ssh_status(self) -> dict[str, str]:
        r = self.send("ssh-status")
        assert r.ok, r
        return dict(kv.split("=", 1) for kv in (r.value or "").split() if "=" in kv)

    def ble_status(self) -> dict[str, str]:
        r = self.send("ble-status")
        assert r.ok, r
        return dict(kv.split("=", 1) for kv in (r.value or "").split() if "=" in kv)

    def migration(self) -> str:
        r = self.send("migration")
        assert r.ok, r
        return (r.value or "").strip()

    # -----------------------------------------------------------------
    # NVS + domain
    # -----------------------------------------------------------------
    def nvs_get(self, ns: str, key: str) -> tuple[str, Any]:
        r = self.send(f"nvs-get {ns} {key}")
        assert r.ok, r
        type_, _, value = (r.value or "").partition(" ")
        if type_ in ("u8", "u16", "u32"):
            return (type_, int(value))
        if type_ == "i32":
            return (type_, int(value))
        return (type_, value)

    def nvs_set(self, ns: str, key: str, type_: str, value: Any) -> None:
        assert self.send(f"nvs-set {ns} {key} {type_} {value}").ok

    def nvs_erase(self, ns: str) -> None:
        assert self.send(f"nvs-erase {ns}").ok

    def wifi_save(self, ssid: str, password: str) -> None:
        assert self.send(f"wifi-save {ssid} {password}").ok

    def wifi_forget(self, ssid: str) -> None:
        assert self.send(f"wifi-forget {ssid}").ok

    def server_upsert(self, creds: dict) -> int:
        import json, base64
        b64 = base64.b64encode(json.dumps(creds).encode()).decode()
        r = self.send(f"server-upsert {b64}")
        assert r.ok, r
        return int(r.value or "-1")

    def server_list(self) -> list[dict]:
        r = self.send("server-list")
        assert r.ok, r
        out = []
        for line in r.data_lines:
            parts = line.split()
            if len(parts) >= 4:
                out.append({
                    "index": int(parts[0]),
                    "name": parts[1],
                    "endpoint": parts[2],
                    "active": parts[3] == "active",
                })
        return out

    def server_delete(self, index: int) -> None:
        assert self.send(f"server-delete {index}").ok

    def server_set_active(self, index: int) -> None:
        assert self.send(f"server-set-active {index}").ok

    def settings_set(self, field: str, value: Any) -> None:
        assert self.send(f"settings-set {field} {value}").ok

    # -----------------------------------------------------------------
    # Filesystem
    # -----------------------------------------------------------------
    def fs_read(self, path: str) -> bytes:
        import base64
        r = self.send(f"fs-read {path}", timeout=60.0)
        assert r.ok, r
        combined = "".join(r.data_lines).replace(" ", "")
        return base64.b64decode(combined)

    def fs_write(self, path: str, data: bytes) -> None:
        import base64
        r = self.send(f"fs-write-begin {path}")
        assert r.ok, r
        token = (r.value or "").strip()
        CHUNK = 3072  # decoded bytes per chunk
        for i in range(0, len(data), CHUNK):
            b64 = base64.b64encode(data[i:i + CHUNK]).decode()
            r2 = self.send(f"fs-write-chunk {token} {b64}", timeout=10.0)
            assert r2.ok, r2
        assert self.send(f"fs-write-commit {token}").ok

    # -----------------------------------------------------------------
    # Runtime
    # -----------------------------------------------------------------
    def ping(self) -> int:
        r = self.send("ping")
        assert r.ok, r
        return int(r.value or "0")

    def reboot(self) -> None:
        # USB-JTAG CDC: esp_restart() drops the USB device off the bus.
        # The host's serial fd becomes invalid until re-enumeration
        # (~1-3s on macOS). Send the command, close our fd, poll for
        # the port to come back, reopen, re-spawn the reader thread.
        try:
            self.send("reboot", timeout=1.0)
        except (TimeoutError, OSError):
            pass
        self._reopen_through_reset()

    def crash(self) -> None:
        try:
            self.send("crash", timeout=0.5)
        except (TimeoutError, OSError):
            pass   # expected — abort() fires without sending OK
        self._reopen_through_reset()

    def _reopen_through_reset(self) -> None:
        """Close the serial port, wait for USB re-enumeration, reopen."""
        port_path = self.ser.port
        baud = self.ser.baudrate
        try:
            self._stop.set()
            self.ser.close()
        except Exception:
            pass
        # Wait for the device file to disappear then reappear.
        deadline = time.time() + 10.0
        while time.time() < deadline:
            if not os.path.exists(port_path):
                break
            time.sleep(0.05)
        while time.time() < deadline:
            if os.path.exists(port_path):
                break
            time.sleep(0.05)
        time.sleep(0.3)   # settle after re-enumeration
        # Reopen with the same settings.
        self.ser = serial.Serial(
            port_path, baud,
            timeout=0.1,
            dsrdtr=False, rtscts=False,
            exclusive=True,
        )
        self._log_buf.clear()
        self._marker_lines.clear()
        self._crashed = False
        self._stop = threading.Event()
        self._reader_thread = threading.Thread(
            target=self._reader_loop, daemon=True
        )
        self._reader_thread.start()
        self.wait_for_boot()
