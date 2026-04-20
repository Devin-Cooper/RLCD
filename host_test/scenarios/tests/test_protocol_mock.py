"""Unit tests for Device.send() parser — no hardware required."""

import threading
import time
import pytest

from device import Device, _MARKER_RE


class FakeSerial:
    def __init__(self, script: bytes):
        self._script = script
        self._pos = 0
        self.writes: list[bytes] = []
    def read(self, n: int) -> bytes:
        if self._pos >= len(self._script):
            time.sleep(0.01)
            return b""
        chunk = self._script[self._pos:self._pos + n]
        self._pos += n
        return chunk
    def write(self, data: bytes) -> int:
        self.writes.append(data)
        return len(data)
    def flush(self): pass
    def close(self): pass
    def setDTR(self, _): pass
    def setRTS(self, _): pass


def test_marker_regex_basic():
    assert _MARKER_RE.match(">>> OK").group(1) == "OK"
    assert _MARKER_RE.match(">>> OK hello").group(2) == "hello"
    assert _MARKER_RE.match(">>> ERR 5 oops").group(2) == "5 oops"
    assert _MARKER_RE.match(">>> DATA whatever").group(2) == "whatever"
    assert _MARKER_RE.match("I (1234) tag: log") is None


def _make_device_with_script(script: bytes) -> Device:
    d = Device.__new__(Device)
    d.ser = FakeSerial(script)
    import collections, threading
    d._log_buf = collections.deque(maxlen=500)
    d._marker_lines = collections.deque()
    d._marker_cond = threading.Condition()
    d._stop = threading.Event()
    d._crashed = False
    d._reader_thread = threading.Thread(target=d._reader_loop, daemon=True)
    d._reader_thread.start()
    return d


def test_send_ok_value():
    d = _make_device_with_script(b">>> OK 12345\r\n")
    r = d.send("ping", timeout=1.0)
    assert r.ok and r.value == "12345"


def test_send_err_with_code():
    d = _make_device_with_script(b">>> ERR 7 payload too long\r\n")
    r = d.send("key-raw FF EE DD CC BB AA 99 88 77", timeout=1.0)
    assert not r.ok
    assert r.code == 7
    assert "payload too long" in (r.value or "")


def test_send_mixed_log_and_data():
    script = (
        b"I (234) log: boot\n"
        b">>> DATA line1\r\n"
        b"I (235) log: more\n"
        b">>> DATA line2\r\n"
        b">>> OK 2\r\n"
    )
    d = _make_device_with_script(script)
    r = d.send("stack", timeout=1.0)
    assert r.ok
    assert r.data_lines == ["line1", "line2"]
