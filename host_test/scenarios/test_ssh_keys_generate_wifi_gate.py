"""Scenario 2: Wi-Fi down -> generate refuses with ERR 5 (wifi down).

Skipped at module level: the firmware's generate_ed25519() consults
esp_wifi_sta_get_ap_info() directly for its gate, so the
system-wifi-state injection helper (which only flips a cached state
machine) does NOT actually flip the real Wi-Fi-up predicate. On hardware
we saw ssh_keys_generate succeed even with "disconnected" injected,
making the load-bearing pytest.raises() a no-op and leaving a stray key
for the next test to trip over.

Hardware-side reproduction of a genuine Wi-Fi-down state would require
wifi-forget + wait-for-disconnect, which destabilizes neighboring
scenarios that assume a connected fresh_device. Until we have an
injection hook that reaches esp_wifi_sta_get_ap_info (or a dedicated
isolated-board slot for this check), the scenario is skipped.
"""
import pytest

pytest.skip(
    "Firmware reads live esp_wifi_sta_get_ap_info; the system-wifi-state "
    "injection doesn't flip the real gate. Hardware-side reproduction "
    "requires wifi-forget which interferes with other scenarios.",
    allow_module_level=True,
)


def test_ssh_keys_generate_wifi_gate(fresh_device):
    # Retained as a placeholder so the scenario reappears in collection
    # once the firmware grows a real injection hook.
    raise RuntimeError("unreachable: module-level skip")
