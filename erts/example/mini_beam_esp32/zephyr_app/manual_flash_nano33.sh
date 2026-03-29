#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOSSAC="${BOSSAC:-$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac}"
BIN="${BIN:-/tmp/os2-nano33-sense-build/zephyr/zephyr.bin}"
PORT="${PORT:-/dev/ttyACM0}"

if [[ ! -x "$BOSSAC" ]]; then
  echo "error: bossac not executable: $BOSSAC" >&2
  exit 1
fi

if [[ ! -f "$BIN" ]]; then
  echo "error: firmware binary not found: $BIN" >&2
  exit 1
fi

echo "manual flash:"
echo "  bossac=$BOSSAC"
echo "  bin=$BIN"
echo "  port=$PORT"

echo "[1/4] touch 1200-baud on $PORT"
stty -F "$PORT" 1200 hupcl

echo "[2/4] double-tap RESET on board now, then press Enter"
read -r _

echo "[3/4] probe bootloader"
"$BOSSAC" -p "$PORT" -i

echo "[4/4] flash firmware"
"$BOSSAC" -p "$PORT" -R -e -w -v -b "$BIN"

echo "done: flash complete"
