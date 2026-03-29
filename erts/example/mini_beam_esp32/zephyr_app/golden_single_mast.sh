#!/usr/bin/env bash
set -euo pipefail

# One-shot single-mast golden run:
# 1) capture serial log via picocom for N seconds
# 2) analyze to csv/json
# 3) run perf regression gate
#
# Usage:
#   ./golden_single_mast.sh
#   ./golden_single_mast.sh --port /dev/ttyACM1 --duration 600 --label df

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PORT="/dev/ttyACM0"
BAUD="115200"
DURATION="600"
LABEL=""
LOG_FILE=""
CSV_FILE=""
JSON_FILE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="${2:?missing value for --port}"
      shift 2
      ;;
    --baud)
      BAUD="${2:?missing value for --baud}"
      shift 2
      ;;
    --duration)
      DURATION="${2:?missing value for --duration}"
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
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if ! command -v picocom >/dev/null 2>&1; then
  echo "error: picocom not found; install with: sudo apt-get install -y picocom" >&2
  exit 2
fi

if [[ ! -e "$PORT" ]]; then
  echo "error: serial port not found: $PORT" >&2
  exit 2
fi

if [[ -z "$LABEL" ]]; then
  LABEL="$(basename "$PORT")"
fi

SCENARIO="golden_${DURATION}s_${LABEL}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"

LOG_FILE="${LOG_FILE:-$SCRIPT_DIR/logs/${SCENARIO}_${TIMESTAMP}.log}"
CSV_FILE="${CSV_FILE:-$SCRIPT_DIR/logs/${SCENARIO}_${TIMESTAMP}.csv}"
JSON_FILE="${JSON_FILE:-$SCRIPT_DIR/logs/${SCENARIO}_${TIMESTAMP}.json}"

mkdir -p "$(dirname "$LOG_FILE")"
mkdir -p "$(dirname "$CSV_FILE")"
mkdir -p "$(dirname "$JSON_FILE")"

echo "[1/3] capture ${DURATION}s on $PORT -> $LOG_FILE"
timeout "${DURATION}s" bash -lc \
  "stdbuf -oL -eL picocom -b '$BAUD' '$PORT' --imap lfcrlf | stdbuf -oL -eL tee '$LOG_FILE'" || true

echo "[2/3] analyze -> csv/json"
"$SCRIPT_DIR/analyze_event_perf.sh" "$LOG_FILE" \
  --scenario "$SCENARIO" \
  --csv "$CSV_FILE" \
  --json "$JSON_FILE"

echo "[3/3] gate check"
"$SCRIPT_DIR/check_perf_regression.sh" "$CSV_FILE" \
  --scenario "$SCENARIO" \
  --min-event-rate-hz "${MIN_EVENT_RATE_HZ:-2.0}" \
  --max-drop-pct "${MAX_DROP_PCT:-0.10}" \
  --min-processed-pct "${MIN_PROCESSED_PCT:-99.0}" \
  --max-sensor-p99-ms "${MAX_SENSOR_P99_MS:-1300}"

echo "done:"
echo "  log=$LOG_FILE"
echo "  csv=$CSV_FILE"
echo "  json=$JSON_FILE"
