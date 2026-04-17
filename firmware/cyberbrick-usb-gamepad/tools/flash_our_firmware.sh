#!/usr/bin/env bash
set -euo pipefail

# Builds and flashes our firmware to ESP32-C3.
# Usage:
#   ./tools/flash_our_firmware.sh [PORT]
# Example:
#   ./tools/flash_our_firmware.sh /dev/tty.usbmodem1101

PORT="${1:-}"

if [[ -n "${PORT}" ]]; then
  pio run -e cyberbrick_esp32c3_usbserial -t upload --upload-port "${PORT}"
else
  pio run -e cyberbrick_esp32c3_usbserial -t upload
fi

echo "Flash complete."
