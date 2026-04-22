"""Scenario 10: ssh-keys-qr-raw encodes the pubkey as a QR matrix; the
frame size and packed-bit count are self-consistent.

The reference-matrix cross-check lives in the host unit tests
(test_ssh_keys_qr_encode.cpp) where the nayuki C output is compared
against a Python-generated reference. This on-hardware scenario
confirms the REPL path produces a parseable frame and that the
modules*modules bit count matches the base64-decoded packed byte
length, which is the load-bearing invariant for the screen
renderer.
"""
import base64


def test_ssh_keys_qr_encode(wifi_device):
    d = wifi_device

    uuid = d.ssh_keys_generate("qr_test")
    try:
        r = d.send(f"ssh-keys-qr-raw {uuid}")
        assert r.ok, r
        assert len(r.data_lines) == 1, f"expected 1 DATA line, got {len(r.data_lines)}"

        line = r.data_lines[0]
        modules_s, sep, packed_b64 = line.partition(" ")
        assert sep == " ", f"no space separator in {line!r}"
        modules = int(modules_s)

        # A QR symbol is square; version 1 is 21×21, version 17 is 85×85.
        # Our encoder is bounded [VERSION_MIN=1, 17].
        assert 21 <= modules <= 85, f"modules={modules} out of expected range"

        packed = base64.b64decode(packed_b64)
        expected_bytes = (modules * modules + 7) // 8
        assert len(packed) == expected_bytes, (
            f"packed size mismatch: got {len(packed)} bytes for "
            f"{modules}x{modules} modules, expected {expected_bytes}"
        )
    finally:
        d.ssh_keys_delete(uuid)
