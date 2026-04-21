#!/usr/bin/env bash
# dev-flash.sh — rewrite only ota_0 (main app) during dev iteration.
#
# DO NOT use `idf.py -C esp32_rendering flash` for this — ESP-IDF defaults
# its app offset to the first APP partition which is `factory` (0x20000),
# so `idf.py flash` would OVERWRITE the recovery firmware with main. This
# script bypasses that by driving esptool directly at 0xA0000.
#
# First-time migration or recovery-side changes: use ./flash.sh instead.
#
# Usage: ./dev-flash.sh [port]             # production config
#        ./dev-flash.sh [port] test        # test-overlay config (+test_console)

set -euo pipefail

PORT="${1:-/dev/cu.usbmodem4101}"
MODE="${2:-prod}"
ROOT="$(cd "$(dirname "$0")" && pwd)"

source "$HOME/.espressif/v5.5.2/esp-idf/export.sh" > /dev/null

if [ "$MODE" = "test" ]; then
    echo "--- build main (test overlay) ---"
    idf.py -C "$ROOT/esp32_rendering" \
        -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test" \
        build
else
    echo "--- build main (production) ---"
    idf.py -C "$ROOT/esp32_rendering" build
fi

echo "--- flash ota_0 only (0xA0000) ---"
python3 -m esptool --chip esp32s3 --port "$PORT" \
    --before usb_reset --after hard_reset \
    write_flash 0xA0000 "$ROOT/esp32_rendering/build/esp32_terminal.bin"

echo "done. Device will boot main from ota_0."
