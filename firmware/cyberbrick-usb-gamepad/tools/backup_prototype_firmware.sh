#!/usr/bin/env bash
set -euo pipefail

# Backs up full ESP32-C3 flash using PlatformIO's esptool package.
# Usage:
#   ./tools/backup_prototype_firmware.sh [PORT] [FLASH_SIZE]
# Example:
#   ./tools/backup_prototype_firmware.sh /dev/tty.usbmodem1101 4MB

PORT="${1:-}"
FLASH_SIZE="${2:-4MB}"

normalize_size_bytes() {
  local input="$1"
  case "${input}" in
    *[mM][bB])
      local n="${input%[mM][bB]}"
      echo $((n * 1024 * 1024))
      ;;
    *[kK][bB])
      local n="${input%[kK][bB]}"
      echo $((n * 1024))
      ;;
    *)
      echo "${input}"
      ;;
  esac
}

if [[ -z "${PORT}" ]]; then
  if compgen -G "/dev/tty.usbmodem*" > /dev/null; then
    PORT="$(ls /dev/tty.usbmodem* | head -n 1)"
  elif compgen -G "/dev/ttyACM*" > /dev/null; then
    PORT="$(ls /dev/ttyACM* | head -n 1)"
  elif compgen -G "/dev/ttyUSB*" > /dev/null; then
    PORT="$(ls /dev/ttyUSB* | head -n 1)"
  else
    echo "No serial device found. Pass port explicitly." >&2
    exit 1
  fi
fi

mkdir -p backups
TS="$(date +%Y%m%d-%H%M%S)"
OUT="backups/prototype-${TS}.bin"

echo "Reading firmware from ${PORT} (${FLASH_SIZE})"
SIZE_BYTES="$(normalize_size_bytes "${FLASH_SIZE}")"
pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32c3 --port "${PORT}" --baud 460800 read_flash 0x0 "${SIZE_BYTES}" "${OUT}"

echo "Backup created: ${OUT}"
shasum -a 256 "${OUT}"
