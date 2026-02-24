#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="${1:-$SCRIPT_DIR/logs/recovery_evidence.log}"
CAPTURE_SECS="${CAPTURE_SECS:-55}"
FLASH_ATTEMPTS="${FLASH_ATTEMPTS:-8}"

mkdir -p "$(dirname "$LOG_FILE")"
rm -f "$LOG_FILE"

echo "[1/3] flash forced-failure image (OS2_FAULT_EVERY_N=1)"
echo "If flash races, double-tap reset right after '[flash] attempt' appears."
ZEPHYR_WS="${ZEPHYR_WS:-/home/io/zephyrproject}" \
OS2_FAULT_EVERY_N=1 \
FLASH_ATTEMPTS="$FLASH_ATTEMPTS" \
BOSSAC="${BOSSAC:-$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac}" \
"$SCRIPT_DIR/reflash_nano33_sense.sh"

echo "[2/3] capture serial logs for ${CAPTURE_SECS}s -> $LOG_FILE"
timeout "${CAPTURE_SECS}s" "$SCRIPT_DIR/start_serial_log.sh" --port "${PORT:-/dev/ttyACM0}" --log "$LOG_FILE" || true

echo "[3/3] validate markers"
"$SCRIPT_DIR/check_recovery_log.sh" "$LOG_FILE"
