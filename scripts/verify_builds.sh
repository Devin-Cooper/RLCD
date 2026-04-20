#!/usr/bin/env bash
# verify_builds.sh — pre-push sanity. Builds recovery + main (both configs)
# and enforces:
# - nvs/otadata/phy_init rows identical in both partition tables
# - recovery binary < 512 KB
#
# Hardware verification (T12-T14 in the factory-recovery plan) is a
# separate manual step — CI can't drive the device.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$HOME/.espressif/v5.5.2/esp-idf/export.sh" > /dev/null

echo "--- partition-table drift check ---"
diff \
    <(grep -E "^(nvs|otadata|phy_init)," "$ROOT/esp32_rendering/partitions.csv") \
    <(grep -E "^(nvs|otadata|phy_init)," "$ROOT/esp32_recovery/partitions.csv") \
    || { echo "ERROR: nvs/otadata/phy_init rows differ between projects — fix before flashing"; exit 2; }
echo "ok"

echo "--- build recovery ---"
idf.py -C "$ROOT/esp32_recovery" build

# Portable file size (wc -c works on both BSD/macOS and GNU/Linux).
RECOVERY_SIZE=$(wc -c < "$ROOT/esp32_recovery/build/esp32_recovery.bin")
if [ "$RECOVERY_SIZE" -ge 524288 ]; then
    echo "ERROR: recovery binary is $RECOVERY_SIZE bytes, exceeds 512 KB slot"
    exit 3
fi
echo "recovery size: $RECOVERY_SIZE bytes ($((RECOVERY_SIZE * 100 / 524288))% of 512 KB)"

echo "--- build main (production config) ---"
idf.py -C "$ROOT/esp32_rendering" build

echo "--- build main (test-overlay config) ---"
idf.py -C "$ROOT/esp32_rendering" \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test" \
    build

echo
echo "all builds green; recovery fits."
