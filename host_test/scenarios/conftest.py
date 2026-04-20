from __future__ import annotations

import os
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
    """fresh_device + one configured server 'test@h:22'."""
    fresh_device.server_upsert({
        "name": "test",
        "host": "h",
        "port": 22,
        "username": "u",
        "password": "p",
    })
    return fresh_device


def pytest_configure(config):
    config.addinivalue_line("markers", "slow: real-time-sensitive tests (BLE timeouts, etc.)")
