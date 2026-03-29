#!/usr/bin/env bash
set -euo pipefail

# Fail-fast preflight:
# 1) ensure os2_caps_v1 term is present/valid
# 2) ensure profile bindings are compatible with caps
#
# Can validate an existing log (--log) or capture a short boot log from serial.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROFILE="$SCRIPT_DIR/profiles/nano33_ble_sense.os2"
LOG_FILE=""
PORT=""
BAUD="115200"
CAPTURE_SECS=12
SUDO_CHOWN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      PROFILE="${2:?missing value for --profile}"
      shift 2
      ;;
    --log)
      LOG_FILE="${2:?missing value for --log}"
      shift 2
      ;;
    --port)
      PORT="${2:?missing value for --port}"
      shift 2
      ;;
    --baud)
      BAUD="${2:?missing value for --baud}"
      shift 2
      ;;
    --capture-secs)
      CAPTURE_SECS="${2:?missing value for --capture-secs}"
      shift 2
      ;;
    --sudo-chown)
      SUDO_CHOWN=1
      shift
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ ! -f "$PROFILE" ]]; then
  echo "ERROR: profile not found: $PROFILE" >&2
  exit 2
fi

if [[ -z "$LOG_FILE" ]]; then
  if [[ -z "$PORT" ]]; then
    echo "ERROR: provide either --log or --port for preflight" >&2
    exit 2
  fi
  mkdir -p "$SCRIPT_DIR/logs"
  LOG_FILE="$SCRIPT_DIR/logs/preflight_$(basename "$PORT")_$(date +%Y%m%d_%H%M%S).log"
  echo "[preflight] capture ${CAPTURE_SECS}s from $PORT -> $LOG_FILE"
  LOGGER_ARGS=(--port "$PORT" --baud "$BAUD" --log "$LOG_FILE")
  if [[ "$SUDO_CHOWN" -eq 1 ]]; then
    LOGGER_ARGS+=(--sudo-chown)
  fi
  timeout "${CAPTURE_SECS}s" "$SCRIPT_DIR/start_serial_log.sh" "${LOGGER_ARGS[@]}" || true
fi

echo "[preflight] validate caps term"
"$SCRIPT_DIR/validate_caps_term.sh" "$LOG_FILE"

echo "[preflight] validate profile bindings"
"$SCRIPT_DIR/validate_profile_bindings.sh" --profile "$PROFILE" --log "$LOG_FILE"

echo "[preflight] PASS profile=$PROFILE log=$LOG_FILE"
