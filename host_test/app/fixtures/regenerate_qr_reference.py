#!/usr/bin/env python3
"""Regenerate qr_reference_ed25519.bin from ed25519_expected_pub.txt.

Fixture format: 2-byte little-endian modules count, then row-major packed
bits of the QR matrix (LSB = first module of the row).

The reference is produced by Python's `qrcode` library (ISO-compliant)
and is compared against the vendored nayuki encoder by the C++ test
test_ssh_keys_qr_encode.cpp. Any drift between the two implementations
(mask selection, data codeword ordering, etc.) surfaces as a per-module
REQUIRE failure there.

Usage:
    python3 -m venv /tmp/rlcd_qr_venv
    /tmp/rlcd_qr_venv/bin/pip install qrcode
    /tmp/rlcd_qr_venv/bin/python3 regenerate_qr_reference.py

(qrcode is deliberately NOT in host_test/scenarios/requirements.txt —
this script runs only when the fixture needs regeneration.)
"""
from __future__ import annotations

from pathlib import Path

import qrcode


def main() -> None:
    here = Path(__file__).resolve().parent
    pub_line = (here / "ed25519_expected_pub.txt").read_text().strip()

    qr = qrcode.QRCode(
        version=None,
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=1,
        border=0,
    )
    qr.add_data(pub_line)
    qr.make(fit=True)

    modules = len(qr.modules)
    bits = bytearray((modules * modules + 7) // 8)
    for y in range(modules):
        for x in range(modules):
            if qr.modules[y][x]:
                bit = y * modules + x
                bits[bit // 8] |= 1 << (bit & 7)

    out = here / "qr_reference_ed25519.bin"
    out.write_bytes(modules.to_bytes(2, "little") + bytes(bits))
    print(f"wrote {out.name}: modules={modules} bytes={len(bits)}")


if __name__ == "__main__":
    main()
