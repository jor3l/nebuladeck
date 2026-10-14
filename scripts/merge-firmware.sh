#!/usr/bin/env bash
# Merges bootloader + partitions + app into one image flashable at 0x0, for one board:
#   scripts/merge-firmware.sh [sc01plus|sc01]   (default sc01plus)
#   esptool write_flash 0x0 dist/nebuladeck-<board>.bin
# Also copies the app-only image used for OTA updates: dist/nebuladeck-<board>-ota.bin.
# ESPTOOL overrides the esptool command (default: python3 -m esptool).
set -euo pipefail
cd "$(dirname "$0")/.."
BOARD="${1:-sc01plus}"
case "$BOARD" in
  sc01plus) CHIP=esp32s3; BOOT_AT=0x0000; SIZE=16MB; FREQ=80m ;;  # S3: bootloader at 0x0
  sc01)     CHIP=esp32;   BOOT_AT=0x1000; SIZE=4MB;  FREQ=40m ;;  # ESP32: bootloader at 0x1000
  *) echo "unknown board: $BOARD (sc01plus or sc01)"; exit 1 ;;
esac
B=firmware/.pio/build/$BOARD
BOOT_APP0="${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"
mkdir -p dist
${ESPTOOL:-python3 -m esptool} --chip "$CHIP" merge_bin -o "dist/nebuladeck-$BOARD.bin" \
  --flash_mode dio --flash_freq "$FREQ" --flash_size "$SIZE" \
  "$BOOT_AT" "$B/bootloader.bin" 0x8000 "$B/partitions.bin" \
  0xe000 "$BOOT_APP0" 0x10000 "$B/firmware.bin"
cp "$B/firmware.bin" "dist/nebuladeck-$BOARD-ota.bin"
echo "wrote dist/nebuladeck-$BOARD.bin and dist/nebuladeck-$BOARD-ota.bin"
