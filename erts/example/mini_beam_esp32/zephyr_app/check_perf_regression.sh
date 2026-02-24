#!/usr/bin/env bash
set -euo pipefail

CSV_FILE="${1:-}"
SCENARIO=""
MIN_EVENT_RATE_HZ=""
MAX_DROP_PCT=""
MIN_PROCESSED_PCT=""
MAX_SENSOR_P99_MS=""

shift || true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --scenario)
      SCENARIO="${2:?missing value for --scenario}"
      shift 2
      ;;
    --min-event-rate-hz)
      MIN_EVENT_RATE_HZ="${2:?missing value for --min-event-rate-hz}"
      shift 2
      ;;
    --max-drop-pct)
      MAX_DROP_PCT="${2:?missing value for --max-drop-pct}"
      shift 2
      ;;
    --min-processed-pct)
      MIN_PROCESSED_PCT="${2:?missing value for --min-processed-pct}"
      shift 2
      ;;
    --max-sensor-p99-ms)
      MAX_SENSOR_P99_MS="${2:?missing value for --max-sensor-p99-ms}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$CSV_FILE" ]]; then
  echo "usage: $0 <metrics.csv> [--scenario <name>] [--min-event-rate-hz <hz>] [--max-drop-pct <pct>] [--min-processed-pct <pct>] [--max-sensor-p99-ms <ms>]" >&2
  exit 2
fi

if [[ ! -f "$CSV_FILE" ]]; then
  echo "error: csv file not found: $CSV_FILE" >&2
  exit 2
fi

get_metric() {
  local key="$1"
  awk -F, -v s="$SCENARIO" -v k="$key" '
    NR == 1 { next }
    (s == "" || $1 == s) && $2 == k {
      print $3
      found = 1
      exit
    }
    END {
      if (!found) {
        exit 1
      }
    }
  ' "$CSV_FILE"
}

check_ge() {
  local name="$1"
  local got="$2"
  local min="$3"
  local ok
  ok="$(awk -v g="$got" -v m="$min" 'BEGIN{ if ((g+0) >= (m+0)) print "1"; else print "0" }')"
  if [[ "$ok" != "1" ]]; then
    echo "FAIL: ${name} expected >= ${min}, got ${got}" >&2
    return 1
  fi
  echo "PASS: ${name}=${got} >= ${min}"
}

check_le() {
  local name="$1"
  local got="$2"
  local max="$3"
  local ok
  ok="$(awk -v g="$got" -v m="$max" 'BEGIN{ if ((g+0) <= (m+0)) print "1"; else print "0" }')"
  if [[ "$ok" != "1" ]]; then
    echo "FAIL: ${name} expected <= ${max}, got ${got}" >&2
    return 1
  fi
  echo "PASS: ${name}=${got} <= ${max}"
}

fail=0

if [[ -n "$MIN_EVENT_RATE_HZ" ]]; then
  if val="$(get_metric event_rate_hz)"; then
    check_ge "event_rate_hz" "$val" "$MIN_EVENT_RATE_HZ" || fail=1
  else
    echo "FAIL: missing metric event_rate_hz" >&2
    fail=1
  fi
fi

if [[ -n "$MAX_DROP_PCT" ]]; then
  if val="$(get_metric mb_drop_over_attempted_pct)"; then
    check_le "mb_drop_over_attempted_pct" "$val" "$MAX_DROP_PCT" || fail=1
  else
    echo "FAIL: missing metric mb_drop_over_attempted_pct" >&2
    fail=1
  fi
fi

if [[ -n "$MIN_PROCESSED_PCT" ]]; then
  if val="$(get_metric mb_processed_over_attempted_pct)"; then
    check_ge "mb_processed_over_attempted_pct" "$val" "$MIN_PROCESSED_PCT" || fail=1
  else
    echo "FAIL: missing metric mb_processed_over_attempted_pct" >&2
    fail=1
  fi
fi

if [[ -n "$MAX_SENSOR_P99_MS" ]]; then
  for sid in 1 2 3; do
    key="sensor_${sid}_p99_ms"
    if val="$(get_metric "$key")"; then
      check_le "$key" "$val" "$MAX_SENSOR_P99_MS" || fail=1
    else
      echo "FAIL: missing metric ${key}" >&2
      fail=1
    fi
  done
fi

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi

echo "PASS: perf regression checks satisfied"
