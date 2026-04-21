from __future__ import annotations

import os
import time

import pytest

from .crash import DeviceCrashError, check_for_crash, extract_coredump, rts_reset
from .device import Device


@pytest.fixture(scope="session")
def device() -> Device:
    d = Device()
    try:
        d.ping()
    except (TimeoutError, AssertionError):
        rts_reset(d)
        d.wait_for_boot()
    # Seed WiFi credentials from env so fresh_device reboots still reach
    # DashboardScreen. Scenarios that need a truly blank device (migration,
    # first-boot) can erase wifi_creds explicitly.
    ssid = os.environ.get("TEST_WIFI_SSID")
    pw = os.environ.get("TEST_WIFI_PASS")
    if ssid and pw:
        try:
            d.wifi_save(ssid, pw)
        except Exception:
            pass
    yield d
    d.close()


def _erase_app_state(d: Device) -> None:
    # Preserve wifi_creds across resets — with USB-JTAG being the only
    # serial path, the device needs network to reach the DashboardScreen
    # state the scenarios assume. Migration/first-boot tests that need
    # a blank slate should erase wifi_creds explicitly.
    #
    # Also wipe /sdcard/servers/*.json so SD-loaded servers don't bleed
    # across resets (non-JSON files like SSH keys are preserved).
    for ns in ("servers", "app_settings", "ssh_creds"):
        d.nvs_erase(ns)
    try:
        d.sd_clear_servers()
    except (AssertionError, TimeoutError):
        pass  # dev boards without SD cards are still runnable


@pytest.fixture
def fresh_device(device: Device, request) -> Device:
    """Reboot, wait for boot, erase servers/app_settings/ssh_creds.

    wifi_creds is preserved so the device boots to DashboardScreen
    (see _erase_app_state docstring).
    """
    _erase_app_state(device)
    device.reboot()
    _erase_app_state(device)
    # Wait for WiFi reconnect + WifiScreen auto-pop + DashboardScreen to be
    # the ONLY screen on the stack. Also drain any leftover events in the
    # input queue by sending a ping sync point.
    import time
    deadline = time.time() + 20.0
    while time.time() < deadline:
        try:
            st = device.wifi_status()
            stack = device.stack()
            if (st.get("state") == "connected"
                    and len(stack) == 1
                    and "DashboardScreen" in stack[0][1]):
                break
        except Exception:
            pass
        time.sleep(0.3)
    # Let pending applyPending() runs settle.
    time.sleep(0.5)
    device.ping()

    # Warmup: post-reboot USB-JTAG CDC enumeration can swallow the first
    # button event even after ping() returns. Sending one btn + ESC drains
    # whatever sink the first event falls into, so the test-driven press
    # lands on a quiescent Dashboard regardless of whether the warmup
    # itself was swallowed.
    device.button("a", "short")
    time.sleep(0.4)
    if "MenuScreen" in device.stack_top():
        device.key_esc()
        time.sleep(0.3)

    def finalizer():
        crashed, marker = check_for_crash(device)
        if crashed:
            backtrace = extract_coredump(device)
            request.node.add_report_section(
                "call", "coredump", f"Crash marker: {marker}\n\n{backtrace}"
            )
            rts_reset(device)
            device.wait_for_boot()
            pytest.fail(f"device crashed mid-test: {marker}")

    request.addfinalizer(finalizer)
    return device


@pytest.fixture
def wifi_device(fresh_device: Device) -> Device:
    """fresh_device + one saved WiFi network (not actually connected)."""
    fresh_device.wifi_save("TestNetwork", "testpass")
    return fresh_device


@pytest.fixture
def one_server_device(fresh_device: Device) -> Device:
    """fresh_device + one configured server at 127.0.0.1.

    Loopback gives an immediate ECONNREFUSED via lwIP, so the SSH client's
    connect attempt fails fast (well under the task watchdog threshold).
    Garbage hostnames like 'h' trigger DNS resolve loops, and TEST-NET-1
    addresses like 192.0.2.1 get TCP-retry-timeout hangs — both paths
    hang ssh_client past the watchdog and crash the device mid-test.
    """
    fresh_device.server_upsert({
        "name": "test",
        "host": "127.0.0.1",
        "port": 22,
        "username": "u",
        "password": "p",
    })
    return fresh_device


def pytest_configure(config):
    config.addinivalue_line("markers", "slow: real-time-sensitive tests (BLE timeouts, etc.)")


import contextlib
import shutil
import socket
import subprocess
import tempfile
import pathlib


def _find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _lan_ip() -> str:
    """Best-effort discover the host's LAN IP reachable by other devices.

    Uses the trick of opening a UDP socket with a routable target —
    this populates getsockname() with the interface the kernel would
    route through, without actually sending any packets.

    Falls back to 127.0.0.1 if discovery fails; the scenarios will
    skip or error out clearly in that case.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # The IP below doesn't need to be reachable — the kernel just
        # picks an outgoing interface to "route" toward it.
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def _sshd_binary() -> str | None:
    for cand in (os.environ.get("SSHD_PATH"), "/usr/sbin/sshd", "/usr/local/sbin/sshd", "/opt/homebrew/sbin/sshd"):
        if cand and os.path.exists(cand):
            return cand
    return None


@pytest.fixture
def loopback_sshd():
    """Spawn a local OpenSSH sshd on 127.0.0.1:ephemeral for SSH scenarios.

    On systems where sshd cannot be started without root (typical on macOS
    with the vendor sshd), the fixture skip()s the test rather than failing.

    Yields:
        {port, user, password, hostkey_path, authorized_keys_path}
    """
    sshd = _sshd_binary()
    if not sshd:
        pytest.skip("no usable sshd; set SSHD_PATH or install OpenSSH server")

    tmpdir = tempfile.mkdtemp(prefix="rlcd-sshd-")
    tmpdir_p = pathlib.Path(tmpdir)
    authorized_keys = tmpdir_p / "authorized_keys"
    hostkey = tmpdir_p / "ssh_host_ed25519_key"
    sshd_config = tmpdir_p / "sshd_config"
    pid_file = tmpdir_p / "sshd.pid"

    try:
        subprocess.check_call([
            "ssh-keygen", "-q", "-t", "ed25519", "-f", str(hostkey), "-N", ""
        ])
        os.chmod(hostkey, 0o600)
        authorized_keys.touch()
        os.chmod(authorized_keys, 0o600)
    except (subprocess.CalledProcessError, OSError) as e:
        shutil.rmtree(tmpdir, ignore_errors=True)
        pytest.skip(f"could not set up sshd host key: {e}")

    port = _find_free_port()
    test_user = os.environ.get("USER") or "nobody"
    lan_ip = _lan_ip()
    sshd_config.write_text(
        f"Port {port}\n"
        f"ListenAddress 0.0.0.0\n"
        f"HostKey {hostkey}\n"
        f"AuthorizedKeysFile {authorized_keys}\n"
        f"PasswordAuthentication yes\n"
        f"PubkeyAuthentication yes\n"
        f"PermitRootLogin no\n"
        f"UsePAM no\n"
        f"PrintMotd no\n"
        f"PrintLastLog no\n"
        f"StrictModes no\n"
        f"PidFile {pid_file}\n"
        f"LogLevel QUIET\n"
    )

    try:
        sshd_proc = subprocess.Popen(
            [sshd, "-f", str(sshd_config), "-D"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
    except OSError as e:
        shutil.rmtree(tmpdir, ignore_errors=True)
        pytest.skip(f"sshd spawn failed: {e}")

    # Wait up to 2s for port to be listening.
    deadline = time.time() + 2.0
    ready = False
    while time.time() < deadline:
        if sshd_proc.poll() is not None:
            break  # sshd exited early — likely privilege error
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                ready = True
                break
        except OSError:
            time.sleep(0.05)

    if not ready:
        stderr = ""
        try:
            _, stderr_b = sshd_proc.communicate(timeout=0.5)
            stderr = stderr_b.decode(errors="replace")[:500]
        except Exception:
            pass
        try:
            sshd_proc.terminate()
        except Exception:
            pass
        shutil.rmtree(tmpdir, ignore_errors=True)
        pytest.skip(f"sshd did not bind port {port}; likely privileges: {stderr}")

    yield {
        "host": lan_ip,
        "port": port,
        "user": test_user,
        "password": "not-actually-used-unless-local-account-accepts-it",
        "hostkey_path": str(hostkey),
        "authorized_keys_path": str(authorized_keys),
    }

    with contextlib.suppress(Exception):
        sshd_proc.terminate()
        sshd_proc.wait(timeout=2)
    shutil.rmtree(tmpdir, ignore_errors=True)
