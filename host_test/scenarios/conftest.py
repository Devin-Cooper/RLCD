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
    yield d
    d.close()


def _erase_nvs_namespaces(d: Device) -> None:
    for ns in ("servers", "wifi_creds", "app_settings", "ssh_creds"):
        d.nvs_erase(ns)


@pytest.fixture
def fresh_device(device: Device, request) -> Device:
    """Reboot, wait for boot, erase servers/wifi_creds/app_settings/ssh_creds.

    Legacy ssh_creds is also erased so migration scenarios don't leak
    state between runs (spec §Why fresh_device erases ssh_creds).
    """
    _erase_nvs_namespaces(device)
    device.reboot()
    _erase_nvs_namespaces(device)

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
