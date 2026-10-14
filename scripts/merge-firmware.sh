#!/usr/bin/env bash
# Merges bootloader + partitions + app into one image flashable at 0x0:
#   esptool --chip esp32s3 write_flash 0x0 nebuladeck-sc01plus.bin
# ESPTOOL overrides the esptool command (default: python3 -m esptool).
set -euo pipefail
cd "$(dirname "$0")/.."
B=firmware/.pio/build/sc01plus
BOOT_APP0="${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"
OUT="${1:-dist/nebuladeck-sc01plus.bin}"
mkdir -p "$(dirname "$OUT")"
${ESPTOOL:-python3 -m esptool} --chip esp32s3 merge_bin -o "$OUT" \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0000 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" \
  0xe000 "$BOOT_APP0" 0x10000 "$B/firmware.bin"
echo "wrote $OUT"
