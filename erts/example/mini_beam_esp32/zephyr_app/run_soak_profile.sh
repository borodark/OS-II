#!/usr/bin/env bash
set -euo pipefail

# Capture a nominal soak profile and emit analyzer artifacts.
# Profiles: 10m, 30m, 60m.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROFILE="10m"
PORT="/dev/ttyACM0"
BAUD="115200"
LOG_FILE=""
CSV_FILE=""
JSON_FILE=""
SKIP_FLASH=0
SUDO_CHOWN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      PROFILE="${2:?missing value for --profile}"
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
    --log)
      LOG_FILE="${2:?missing value for --log}"
      shift 2
      ;;
    --csv)
      CSV_FILE="${2:?missing value for --csv}"
      shift 2
      ;;
    --json)
      JSON_FILE="${2:?missing value for --json}"
      shift 2
      ;;
    --skip-flash)
      SKIP_FLASH=1
      shift
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

case "$PROFILE" in
  10m) DURATION_SECS=600 ;;
  30m) DURATION_SECS=1800 ;;
  60m) DURATION_SECS=3600 ;;
  *)
    echo "error: unsupported profile '$PROFILE' (use 10m|30m|60m)" >&2
    exit 2
    ;;
esac

LOG_FILE="${LOG_FILE:-$SCRIPT_DIR/logs/nominal_soak_${PROFILE}.log}"
CSV_FILE="${CSV_FILE:-$SCRIPT_DIR/logs/nominal_soak_${PROFILE}.csv}"
JSON_FILE="${JSON_FILE:-$SCRIPT_DIR/logs/nominal_soak_${PROFILE}.json}"

mkdir -p "$(dirname "$LOG_FILE")"
mkdir -p "$(dirname "$CSV_FILE")"
mkdir -p "$(dirname "$JSON_FILE")"

if [[ "$SKIP_FLASH" -ne 1 ]]; then
  echo "[1/4] flash nominal firmware"
  FLASH_ATTEMPTS="${FLASH_ATTEMPTS:-6}" SUDO_CHOWN="$SUDO_CHOWN" "$SCRIPT_DIR/reflash_nano33_sense.sh"
else
  echo "[1/4] skip flash"
fi

echo "[2/4] capture serial log for ${DURATION_SECS}s -> $LOG_FILE"
LOGGER_ARGS=(--port "$PORT" --baud "$BAUD" --log "$LOG_FILE")
if [[ "$SUDO_CHOWN" -eq 1 ]]; then
  LOGGER_ARGS+=(--sudo-chown)
fi

timeout "${DURATION_SECS}s" "$SCRIPT_DIR/start_serial_log.sh" "${LOGGER_ARGS[@]}" || true

echo "[3/4] analyze log -> csv/json"
"$SCRIPT_DIR/analyze_event_perf.sh" "$LOG_FILE" \
  --scenario "nominal_soak_${PROFILE}" \
  --csv "$CSV_FILE" \
  --json "$JSON_FILE"

echo "[4/4] check nominal thresholds"
"$SCRIPT_DIR/check_perf_regression.sh" "$CSV_FILE" \
  --scenario "nominal_soak_${PROFILE}" \
  --min-event-rate-hz "${MIN_EVENT_RATE_HZ:-2.0}" \
  --max-drop-pct "${MAX_DROP_PCT:-0.10}" \
  --min-processed-pct "${MIN_PROCESSED_PCT:-99.0}" \
  --max-sensor-p99-ms "${MAX_SENSOR_P99_MS:-1300}"

echo "done: profile=$PROFILE log=$LOG_FILE csv=$CSV_FILE json=$JSON_FILE"
