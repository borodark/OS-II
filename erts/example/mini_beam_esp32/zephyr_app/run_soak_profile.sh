#!/usr/bin/env bash
set -euo pipefail

# Capture a nominal soak profile and emit analyzer artifacts.
# Profiles: 10m, 30m, 60m.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROFILE="10m"
PORT="/dev/ttyACM0"
BAUD="115200"
LABEL=""
LOG_FILE=""
CSV_FILE=""
JSON_FILE=""
PROFILE_FILE="$SCRIPT_DIR/profiles/nano33_ble_sense.os2"
SKIP_FLASH=0
SKIP_PREFLIGHT=0
PREFLIGHT_CAPTURE_SECS=12
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
    --label)
      LABEL="${2:?missing value for --label}"
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
    --profile-file)
      PROFILE_FILE="${2:?missing value for --profile-file}"
      shift 2
      ;;
    --skip-flash)
      SKIP_FLASH=1
      shift
      ;;
    --skip-preflight)
      SKIP_PREFLIGHT=1
      shift
      ;;
    --preflight-capture-secs)
      PREFLIGHT_CAPTURE_SECS="${2:?missing value for --preflight-capture-secs}"
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

case "$PROFILE" in
  10m) DURATION_SECS=600 ;;
  30m) DURATION_SECS=1800 ;;
  60m) DURATION_SECS=3600 ;;
  *)
    echo "error: unsupported profile '$PROFILE' (use 10m|30m|60m)" >&2
    exit 2
    ;;
esac

if [[ -z "$LABEL" ]]; then
  LABEL="$(basename "$PORT")"
fi

SCENARIO="nominal_soak_${PROFILE}_${LABEL}"
LOG_FILE="${LOG_FILE:-$SCRIPT_DIR/logs/${SCENARIO}.log}"
CSV_FILE="${CSV_FILE:-$SCRIPT_DIR/logs/${SCENARIO}.csv}"
JSON_FILE="${JSON_FILE:-$SCRIPT_DIR/logs/${SCENARIO}.json}"

mkdir -p "$(dirname "$LOG_FILE")"
mkdir -p "$(dirname "$CSV_FILE")"
mkdir -p "$(dirname "$JSON_FILE")"

if [[ "$SKIP_FLASH" -ne 1 ]]; then
  echo "[1/5] flash nominal firmware on $PORT"
  FLASH_ATTEMPTS="${FLASH_ATTEMPTS:-6}" PORT="$PORT" SUDO_CHOWN="$SUDO_CHOWN" "$SCRIPT_DIR/reflash_nano33_sense.sh"
else
  echo "[1/5] skip flash"
fi

if [[ "$SKIP_PREFLIGHT" -eq 1 ]]; then
  echo "[2/5] skip preflight"
else
  echo "[2/5] preflight caps/profile guard"
  PREFLIGHT_ARGS=(--profile "$PROFILE_FILE" --port "$PORT" --capture-secs "$PREFLIGHT_CAPTURE_SECS" --baud "$BAUD")
  if [[ "$SUDO_CHOWN" -eq 1 ]]; then
    PREFLIGHT_ARGS+=(--sudo-chown)
  fi
  if ! "$SCRIPT_DIR/preflight_profile_check.sh" "${PREFLIGHT_ARGS[@]}"; then
    echo "error: preflight failed; refusing soak run." >&2
    echo "fix profile/caps mismatch or pass --skip-preflight to override." >&2
    exit 1
  fi
fi

echo "[3/5] capture serial log for ${DURATION_SECS}s -> $LOG_FILE"
LOGGER_ARGS=(--port "$PORT" --baud "$BAUD" --log "$LOG_FILE")
if [[ "$SUDO_CHOWN" -eq 1 ]]; then
  LOGGER_ARGS+=(--sudo-chown)
fi

timeout "${DURATION_SECS}s" "$SCRIPT_DIR/start_serial_log.sh" "${LOGGER_ARGS[@]}" || true

echo "[4/5] analyze log -> csv/json"
"$SCRIPT_DIR/analyze_event_perf.sh" "$LOG_FILE" \
  --scenario "$SCENARIO" \
  --csv "$CSV_FILE" \
  --json "$JSON_FILE"

echo "[5/5] check nominal thresholds"
"$SCRIPT_DIR/check_perf_regression.sh" "$CSV_FILE" \
  --scenario "$SCENARIO" \
  --min-event-rate-hz "${MIN_EVENT_RATE_HZ:-2.0}" \
  --max-drop-pct "${MAX_DROP_PCT:-0.10}" \
  --min-processed-pct "${MIN_PROCESSED_PCT:-99.0}" \
  --max-sensor-p99-ms "${MAX_SENSOR_P99_MS:-1300}"

echo "done: profile=$PROFILE label=$LABEL port=$PORT log=$LOG_FILE csv=$CSV_FILE json=$JSON_FILE"
