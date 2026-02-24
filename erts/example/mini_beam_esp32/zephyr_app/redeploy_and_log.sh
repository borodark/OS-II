#!/usr/bin/env bash
set -euo pipefail

# One-command flow:
# 1) rebuild + flash
# 2) start reliable serial logger (auto-reconnect)
#
# Usage:
#   ./redeploy_and_log.sh
#   ./redeploy_and_log.sh --port /dev/ttyACM1 --log logs/nano33.log --sudo-chown

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="/dev/ttyACM0"
LOG_FILE="$SCRIPT_DIR/logs/nano33.log"
SUDO_CHOWN=0
WAIT_SECS=8

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="${2:?missing value for --port}"
      shift 2
      ;;
    --log)
      LOG_FILE="${2:?missing value for --log}"
      shift 2
      ;;
    --sudo-chown)
      SUDO_CHOWN=1
      shift
      ;;
    --wait-secs)
      WAIT_SECS="${2:?missing value for --wait-secs}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$(dirname "$LOG_FILE")"

echo "[1/2] Rebuild + flash"
if [[ "$SUDO_CHOWN" -eq 1 ]]; then
  PORT="$PORT" SUDO_CHOWN=1 "$SCRIPT_DIR/reflash_nano33_sense.sh"
else
  PORT="$PORT" "$SCRIPT_DIR/reflash_nano33_sense.sh"
fi

echo "[post-flash] Wait for $PORT to reappear (up to ${WAIT_SECS}s)"
for _ in $(seq 1 "$WAIT_SECS"); do
  [[ -e "$PORT" ]] && break
  sleep 1
done

echo "[2/2] Start serial logger"
LOGGER_ARGS=(--port "$PORT" --log "$LOG_FILE")
if [[ "$SUDO_CHOWN" -eq 1 ]]; then
  LOGGER_ARGS+=(--sudo-chown)
fi

echo "Logger file: $LOG_FILE"
echo "Tail in another terminal:"
echo "  tail -n +1 -F \"$LOG_FILE\" --sleep-interval=0.1"
exec "$SCRIPT_DIR/start_serial_log.sh" "${LOGGER_ARGS[@]}"
